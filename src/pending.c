#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pending.h"

static pending_request_t slots[MAX_PENDING];

void pending_init(void)
{
	memset(slots, 0, sizeof(slots));
}

static int generate_request_id(char *out)
{
	unsigned char buf[16];
	FILE *f = fopen("/dev/urandom", "r");
	if (!f)
		return -1;
	if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
		fclose(f);
		return -1;
	}
	fclose(f);

	for (int i = 0; i < 16; i++)
		sprintf(out + i * 2, "%02x", buf[i]);
	out[32] = '\0';
	return 0;
}

int pending_create(const char *device_id, char *request_id_out)
{
	for (int i = 0; i < MAX_PENDING; i++) {
		if (!slots[i].in_use) {
			memset(&slots[i], 0, sizeof(pending_request_t));
			slots[i].in_use = 1;
			slots[i].completed = 0;
			pthread_mutex_init(&slots[i].mutex, NULL);
			pthread_cond_init(&slots[i].cond, NULL);
			snprintf(slots[i].device_id, sizeof(slots[i].device_id),
				 "%s", device_id);

			if (generate_request_id(slots[i].request_id) != 0) {
				slots[i].in_use = 0;
				return -1;
			}
			memcpy(request_id_out, slots[i].request_id,
			       REQUEST_ID_LEN);
			return i;
		}
	}
	return -1;
}

int pending_wait(int slot, int timeout_sec,
		 double *lat, double *lng, float *accuracy)
{
	pending_request_t *r = &slots[slot];
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_sec;

	pthread_mutex_lock(&r->mutex);
	while (!r->completed) {
		int rc = pthread_cond_timedwait(&r->cond, &r->mutex, &ts);
		if (rc != 0) {
			pthread_mutex_unlock(&r->mutex);
			return -1;
		}
	}
	*lat = r->latitude;
	*lng = r->longitude;
	*accuracy = r->accuracy;
	pthread_mutex_unlock(&r->mutex);
	return 0;
}

int pending_complete(const char *request_id, double lat, double lng,
		     float accuracy)
{
	for (int i = 0; i < MAX_PENDING; i++) {
		if (slots[i].in_use &&
		    strcmp(slots[i].request_id, request_id) == 0) {
			pthread_mutex_lock(&slots[i].mutex);
			slots[i].latitude = lat;
			slots[i].longitude = lng;
			slots[i].accuracy = accuracy;
			slots[i].completed = 1;
			pthread_cond_signal(&slots[i].cond);
			pthread_mutex_unlock(&slots[i].mutex);
			return 0;
		}
	}
	return -1;
}

void pending_release(int slot)
{
	if (slot >= 0 && slot < MAX_PENDING) {
		pthread_mutex_destroy(&slots[slot].mutex);
		pthread_cond_destroy(&slots[slot].cond);
		slots[slot].in_use = 0;
	}
}
