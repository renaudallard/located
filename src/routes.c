#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routes.h"
#include "device_db.h"
#include "http_server.h"
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
	cJSON *arr = db_list(ctx->db);
	if (!arr)
		return respond_error(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
				     "database error");

	return respond_json(conn, MHD_HTTP_OK, arr);
}
