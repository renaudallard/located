#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_server.h"
#include "routes.h"

/* Per-connection state for POST body accumulation. */
typedef struct {
	char *data;
	size_t size;
} post_data_t;

enum MHD_Result respond_json(struct MHD_Connection *conn, unsigned int status,
			     cJSON *json)
{
	char *body = cJSON_PrintUnformatted(json);
	cJSON_Delete(json);

	struct MHD_Response *resp =
		MHD_create_response_from_buffer(strlen(body), body,
						MHD_RESPMEM_MUST_FREE);
	MHD_add_response_header(resp, "Content-Type", "application/json");
	enum MHD_Result ret = MHD_queue_response(conn, status, resp);
	MHD_destroy_response(resp);
	return ret;
}

enum MHD_Result respond_error(struct MHD_Connection *conn, unsigned int status,
			      const char *message)
{
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "error", message);
	return respond_json(conn, status, obj);
}

static enum MHD_Result dispatch(void *cls, struct MHD_Connection *conn,
				const char *url, const char *method,
				const char *version,
				const char *upload_data,
				size_t *upload_data_size, void **con_cls)
{
	(void)version;
	route_ctx_t *ctx = cls;

	/* First call: allocate per-connection state. */
	if (*con_cls == NULL) {
		post_data_t *pd = calloc(1, sizeof(post_data_t));
		*con_cls = pd;
		return MHD_YES;
	}

	post_data_t *pd = *con_cls;

	/* Accumulate POST/PUT body data. */
	if (*upload_data_size > 0) {
		char *new_data = realloc(pd->data, pd->size + *upload_data_size + 1);
		if (!new_data)
			return MHD_NO;
		pd->data = new_data;
		memcpy(pd->data + pd->size, upload_data, *upload_data_size);
		pd->size += *upload_data_size;
		pd->data[pd->size] = '\0';
		*upload_data_size = 0;
		return MHD_YES;
	}

	/* Body complete — route the request. */
	enum MHD_Result ret;

	if (strcmp(method, "POST") == 0 &&
	    strcmp(url, "/api/register") == 0) {
		ret = route_register(conn, ctx, pd->data);
	} else if (strcmp(method, "DELETE") == 0 &&
		   strncmp(url, "/api/register/", 14) == 0 &&
		   strlen(url) > 14) {
		const char *id = url + 14;
		ret = route_unregister(conn, ctx, id);
	} else if (strcmp(method, "GET") == 0 &&
		   strcmp(url, "/api/devices") == 0) {
		ret = route_devices(conn, ctx);
	} else {
		ret = respond_error(conn, MHD_HTTP_NOT_FOUND, "not found");
	}

	free(pd->data);
	free(pd);
	*con_cls = NULL;

	return ret;
}

struct MHD_Daemon *http_server_start(int port, route_ctx_t *ctx)
{
	struct MHD_Daemon *d = MHD_start_daemon(
		MHD_USE_INTERNAL_POLLING_THREAD,
		(uint16_t)port, NULL, NULL,
		&dispatch, ctx,
		MHD_OPTION_END);

	if (!d)
		fprintf(stderr, "http: failed to start on port %d\n", port);
	else
		fprintf(stderr, "http: listening on port %d\n", port);

	return d;
}

void http_server_stop(struct MHD_Daemon *d)
{
	if (d)
		MHD_stop_daemon(d);
}
