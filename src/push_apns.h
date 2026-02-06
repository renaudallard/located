#ifndef PUSH_APNS_H
#define PUSH_APNS_H

#include "config.h"

/*
 * Send an APNs silent push to a device via HTTP/2.
 * token: the device's APNs device token (hex string).
 * request_id: the pending request ID.
 * server_url: callback URL base.
 * device_id: device ID for the callback path.
 * Returns 0 on success, -1 on error.
 */
int push_apns_send(const config_t *cfg, const char *token,
		   const char *request_id, const char *server_url,
		   const char *device_id);

#endif
