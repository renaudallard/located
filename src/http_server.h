#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <microhttpd.h>
#include "routes.h"
#include "cJSON.h"

/*
 * Start the HTTP server on the given port.
 * Returns the MHD_Daemon pointer, or NULL on error.
 */
struct MHD_Daemon *http_server_start(int port, route_ctx_t *ctx);

/* Stop the HTTP server. */
void http_server_stop(struct MHD_Daemon *d);

/* Send a JSON response. Takes ownership of json (calls cJSON_Delete). */
enum MHD_Result respond_json(struct MHD_Connection *conn, unsigned int status,
			     cJSON *json);

/* Send a JSON error response. */
enum MHD_Result respond_error(struct MHD_Connection *conn, unsigned int status,
			      const char *message);

#endif
