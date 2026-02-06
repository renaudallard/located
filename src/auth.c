#include <string.h>

#include "auth.h"

int auth_check_api_key(struct MHD_Connection *conn, const char *api_key)
{
	if (!api_key)
		return 1;

	const char *hdr = MHD_lookup_connection_value(conn, MHD_HEADER_KIND,
						      "Authorization");
	if (!hdr)
		return 0;

	/* Expect "Bearer <key>" */
	if (strncmp(hdr, "Bearer ", 7) != 0)
		return 0;

	return strcmp(hdr + 7, api_key) == 0;
}

int auth_check_device_secret(struct MHD_Connection *conn,
			     const char *expected_secret)
{
	if (!expected_secret)
		return 0;

	const char *hdr = MHD_lookup_connection_value(conn, MHD_HEADER_KIND,
						      "X-Device-Secret");
	if (!hdr)
		return 0;

	return strcmp(hdr, expected_secret) == 0;
}
