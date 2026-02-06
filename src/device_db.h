#ifndef DEVICE_DB_H
#define DEVICE_DB_H

#include <sqlite3.h>
#include "cJSON.h"

/* Open (or create) the database at path. Returns 0 on success. */
int db_open(sqlite3 **db, const char *path);

/* Close the database. */
void db_close(sqlite3 *db);

/*
 * Register a device. Generates a secret from /dev/urandom.
 * Returns a cJSON object with the device info including secret,
 * or NULL on error. Caller must cJSON_Delete the result.
 */
cJSON *db_register(sqlite3 *db, const char *id, const char *token,
		   const char *platform, const char *name);

/* Unregister a device by id. Returns 0 on success, -1 if not found. */
int db_unregister(sqlite3 *db, const char *id);

/* List all devices. Returns a cJSON array. Caller must cJSON_Delete. */
cJSON *db_list(sqlite3 *db);

/* Get a single device by id. Returns cJSON object or NULL. Caller must cJSON_Delete. */
cJSON *db_get(sqlite3 *db, const char *id);

#endif
