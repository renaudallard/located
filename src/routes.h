#ifndef ROUTES_H
#define ROUTES_H

#include <microhttpd.h>
#include <sqlite3.h>
#include "config.h"

/*
 * Route context passed to handlers.
 */
typedef struct {
	sqlite3 *db;
	const config_t *cfg;
} route_ctx_t;

/* POST /api/register */
enum MHD_Result route_register(struct MHD_Connection *conn,
			       route_ctx_t *ctx, const char *body);

/* DELETE /api/register/<id> */
enum MHD_Result route_unregister(struct MHD_Connection *conn,
				 route_ctx_t *ctx, const char *id);

/* GET /api/devices (admin, requires API key) */
enum MHD_Result route_devices(struct MHD_Connection *conn, route_ctx_t *ctx);

/* POST /api/locate/<id> (admin, requires API key) */
enum MHD_Result route_locate(struct MHD_Connection *conn,
			     route_ctx_t *ctx, const char *id);

/* POST /api/location/<id> (device callback, requires device secret) */
enum MHD_Result route_location(struct MHD_Connection *conn,
			       route_ctx_t *ctx, const char *id,
			       const char *body);

#endif
