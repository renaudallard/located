#ifndef PENDING_H
#define PENDING_H

#include <pthread.h>

#define REQUEST_ID_LEN 33  /* 32 hex chars + NUL */
#define MAX_PENDING    64

typedef struct {
	char request_id[REQUEST_ID_LEN];
	char device_id[128];
	double latitude;
	double longitude;
	float accuracy;
	int completed;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int in_use;
} pending_request_t;

/* Initialize the pending store. Call once at startup. */
void pending_init(void);

/*
 * Create a pending request for a device. Generates a random request_id.
 * Returns the slot index, or -1 if full.
 */
int pending_create(const char *device_id, char *request_id_out);

/*
 * Wait for a pending request to complete.
 * timeout_sec: max seconds to wait.
 * Returns 0 if completed, -1 on timeout.
 * On success, fills lat, lng, accuracy.
 */
int pending_wait(int slot, int timeout_sec,
		 double *lat, double *lng, float *accuracy);

/*
 * Complete a pending request (called by device callback).
 * Matches by request_id.
 * Returns 0 on success, -1 if not found.
 */
int pending_complete(const char *request_id, double lat, double lng,
		     float accuracy);

/* Release a pending slot. */
void pending_release(int slot);

#endif
