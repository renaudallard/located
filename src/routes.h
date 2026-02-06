#ifndef ROUTES_H
#define ROUTES_H

#include <microhttpd.h>
#include <sqlite3.h>

/*
 * Route context passed to handlers.
 */
typedef struct {
	sqlite3 *db;
	const char *api_key;
} route_ctx_t;

/*
 * Handle POST /api/register
 * Body: {"id":"...", "token":"...", "platform":"android|ios", "name":"..."}
 */
enum MHD_Result route_register(struct MHD_Connection *conn,
			       route_ctx_t *ctx, const char *body);

/*
 * Handle DELETE /api/register/<id>
 */
enum MHD_Result route_unregister(struct MHD_Connection *conn,
				 route_ctx_t *ctx, const char *id);

/*
 * Handle GET /api/devices
 */
enum MHD_Result route_devices(struct MHD_Connection *conn, route_ctx_t *ctx);

#endif
