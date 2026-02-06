#ifndef AUTH_H
#define AUTH_H

#include <microhttpd.h>

/*
 * Check the Authorization header against the expected API key.
 * Expected format: "Bearer <api_key>"
 * Returns 1 if valid, 0 if invalid/missing.
 * If api_key is NULL, always returns 1 (auth disabled).
 */
int auth_check_api_key(struct MHD_Connection *conn, const char *api_key);

/*
 * Check the X-Device-Secret header against the expected device secret.
 * Returns 1 if valid, 0 if invalid/missing.
 */
int auth_check_device_secret(struct MHD_Connection *conn,
			     const char *expected_secret);

#endif
