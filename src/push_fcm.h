#ifndef PUSH_FCM_H
#define PUSH_FCM_H

#include "config.h"

/*
 * Send an FCM data message to a device.
 * token: the device's FCM registration token.
 * request_id: the pending request ID to include in the payload.
 * server_url: callback URL base (e.g. "http://your-server:8080").
 * device_id: device ID for the callback path.
 * Returns 0 on success, -1 on error.
 */
int push_fcm_send(const config_t *cfg, const char *token,
		  const char *request_id, const char *server_url,
		  const char *device_id);

#endif
