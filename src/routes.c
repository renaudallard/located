#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routes.h"
#include "device_db.h"
#include "http_server.h"
#include "auth.h"
#include "pending.h"
#include "push_fcm.h"
#include "push_apns.h"
#include "cJSON.h"

enum MHD_Result route_register(struct MHD_Connection *conn,
			       route_ctx_t *ctx, const char *body)
{
	cJSON *root = cJSON_Parse(body);
	if (!root)
		return respond_error(conn, MHD_HTTP_BAD_REQUEST,
				     "invalid JSON");

	cJSON *jid = cJSON_GetObjectItemCaseSensitive(root, "id");
	cJSON *jtok = cJSON_GetObjectItemCaseSensitive(root, "token");
	cJSON *jplat = cJSON_GetObjectItemCaseSensitive(root, "platform");
	cJSON *jname = cJSON_GetObjectItemCaseSensitive(root, "name");

	if (!cJSON_IsString(jid) || !cJSON_IsString(jtok) ||
	    !cJSON_IsString(jplat)) {
		cJSON_Delete(root);
		return respond_error(conn, MHD_HTTP_BAD_REQUEST,
				     "missing required fields: id, token, platform");
	}

	const char *name = NULL;
	if (cJSON_IsString(jname))
		name = jname->valuestring;

	cJSON *result = db_register(ctx->db, jid->valuestring,
				    jtok->valuestring,
				    jplat->valuestring, name);
	cJSON_Delete(root);

	if (!result)
		return respond_error(conn, MHD_HTTP_CONFLICT,
				     "device already registered or invalid platform");

	return respond_json(conn, MHD_HTTP_CREATED, result);
}

enum MHD_Result route_unregister(struct MHD_Connection *conn,
				 route_ctx_t *ctx, const char *id)
{
	if (db_unregister(ctx->db, id) != 0)
		return respond_error(conn, MHD_HTTP_NOT_FOUND,
				     "device not found");

	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "status", "deleted");
	return respond_json(conn, MHD_HTTP_OK, obj);
}

enum MHD_Result route_devices(struct MHD_Connection *conn, route_ctx_t *ctx)
{
	if (!auth_check_api_key(conn, ctx->cfg->api_key))
		return respond_error(conn, MHD_HTTP_UNAUTHORIZED,
				     "invalid or missing API key");

	cJSON *arr = db_list(ctx->db);
	if (!arr)
		return respond_error(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
				     "database error");

	return respond_json(conn, MHD_HTTP_OK, arr);
}

enum MHD_Result route_locate(struct MHD_Connection *conn,
			     route_ctx_t *ctx, const char *id)
{
	if (!auth_check_api_key(conn, ctx->cfg->api_key))
		return respond_error(conn, MHD_HTTP_UNAUTHORIZED,
				     "invalid or missing API key");

	/* Look up device. */
	cJSON *dev = db_get(ctx->db, id);
	if (!dev)
		return respond_error(conn, MHD_HTTP_NOT_FOUND,
				     "device not found");

	const char *token = cJSON_GetStringValue(
		cJSON_GetObjectItemCaseSensitive(dev, "token"));
	const char *platform = cJSON_GetStringValue(
		cJSON_GetObjectItemCaseSensitive(dev, "platform"));

	if (!token || !platform) {
		cJSON_Delete(dev);
		return respond_error(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
				     "device record corrupt");
	}

	/* Create pending request. */
	char request_id[33];
	int slot = pending_create(id, request_id);
	if (slot < 0) {
		cJSON_Delete(dev);
		return respond_error(conn, MHD_HTTP_SERVICE_UNAVAILABLE,
				     "too many pending requests");
	}

	/* Determine callback URL. */
	const char *server_url = ctx->cfg->server_url;
	char url_buf[256];
	if (!server_url) {
		snprintf(url_buf, sizeof(url_buf),
			 "http://localhost:%d", ctx->cfg->listen_port);
		server_url = url_buf;
	}

	/* Send push notification. */
	int push_rc;
	if (strcmp(platform, "android") == 0)
		push_rc = push_fcm_send(ctx->cfg, token, request_id,
					server_url, id);
	else
		push_rc = push_apns_send(ctx->cfg, token, request_id,
					 server_url, id);

	cJSON_Delete(dev);

	if (push_rc != 0) {
		pending_release(slot);
		return respond_error(conn, MHD_HTTP_BAD_GATEWAY,
				     "failed to send push notification");
	}

	/* Wait for device to respond. */
	double lat, lng;
	float accuracy;
	int rc = pending_wait(slot, ctx->cfg->locate_timeout,
			      &lat, &lng, &accuracy);
	pending_release(slot);

	if (rc != 0)
		return respond_error(conn, MHD_HTTP_GATEWAY_TIMEOUT,
				     "device did not respond in time");

	cJSON *result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "device_id", id);
	cJSON_AddNumberToObject(result, "latitude", lat);
	cJSON_AddNumberToObject(result, "longitude", lng);
	cJSON_AddNumberToObject(result, "accuracy", accuracy);
	return respond_json(conn, MHD_HTTP_OK, result);
}

enum MHD_Result route_location(struct MHD_Connection *conn,
			       route_ctx_t *ctx, const char *id,
			       const char *body)
{
	/* Validate device secret. */
	cJSON *dev = db_get(ctx->db, id);
	if (!dev)
		return respond_error(conn, MHD_HTTP_NOT_FOUND,
				     "device not found");

	const char *secret = cJSON_GetStringValue(
		cJSON_GetObjectItemCaseSensitive(dev, "secret"));
	if (!auth_check_device_secret(conn, secret)) {
		cJSON_Delete(dev);
		return respond_error(conn, MHD_HTTP_UNAUTHORIZED,
				     "invalid device secret");
	}
	cJSON_Delete(dev);

	/* Parse location payload. */
	cJSON *root = cJSON_Parse(body);
	if (!root)
		return respond_error(conn, MHD_HTTP_BAD_REQUEST,
				     "invalid JSON");

	cJSON *jrid = cJSON_GetObjectItemCaseSensitive(root, "request_id");
	cJSON *jlat = cJSON_GetObjectItemCaseSensitive(root, "lat");
	cJSON *jlng = cJSON_GetObjectItemCaseSensitive(root, "lng");
	cJSON *jacc = cJSON_GetObjectItemCaseSensitive(root, "accuracy");

	if (!cJSON_IsString(jrid) || !cJSON_IsNumber(jlat) ||
	    !cJSON_IsNumber(jlng)) {
		cJSON_Delete(root);
		return respond_error(conn, MHD_HTTP_BAD_REQUEST,
				     "missing fields: request_id, lat, lng");
	}

	float accuracy = 0;
	if (cJSON_IsNumber(jacc))
		accuracy = (float)jacc->valuedouble;

	int rc = pending_complete(jrid->valuestring,
				  jlat->valuedouble, jlng->valuedouble,
				  accuracy);
	cJSON_Delete(root);

	if (rc != 0)
		return respond_error(conn, MHD_HTTP_NOT_FOUND,
				     "no pending request with that id");

	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "status", "ok");
	return respond_json(conn, MHD_HTTP_OK, obj);
}
