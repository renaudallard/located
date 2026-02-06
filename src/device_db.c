#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device_db.h"

static const char *SCHEMA =
	"CREATE TABLE IF NOT EXISTS devices ("
	"  id         TEXT PRIMARY KEY,"
	"  token      TEXT NOT NULL,"
	"  platform   TEXT NOT NULL CHECK(platform IN ('android', 'ios')),"
	"  name       TEXT,"
	"  secret     TEXT NOT NULL,"
	"  created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))"
	");";

int db_open(sqlite3 **db, const char *path)
{
	int rc = sqlite3_open(path, db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "db: cannot open %s: %s\n", path,
			sqlite3_errmsg(*db));
		return -1;
	}

	char *err = NULL;
	rc = sqlite3_exec(*db, SCHEMA, NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "db: schema error: %s\n", err);
		sqlite3_free(err);
		sqlite3_close(*db);
		return -1;
	}

	return 0;
}

void db_close(sqlite3 *db)
{
	if (db)
		sqlite3_close(db);
}

static int generate_secret(char *out, size_t hex_len)
{
	size_t nbytes = hex_len / 2;
	unsigned char buf[nbytes];

	FILE *f = fopen("/dev/urandom", "r");
	if (!f)
		return -1;

	if (fread(buf, 1, nbytes, f) != nbytes) {
		fclose(f);
		return -1;
	}
	fclose(f);

	for (size_t i = 0; i < nbytes; i++)
		sprintf(out + i * 2, "%02x", buf[i]);
	out[hex_len] = '\0';

	return 0;
}

cJSON *db_register(sqlite3 *db, const char *id, const char *token,
		   const char *platform, const char *name)
{
	char secret[33];
	if (generate_secret(secret, 32) != 0) {
		fprintf(stderr, "db: failed to generate secret\n");
		return NULL;
	}

	const char *sql =
		"INSERT INTO devices (id, token, platform, name, secret) "
		"VALUES (?, ?, ?, ?, ?);";

	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "db: prepare error: %s\n", sqlite3_errmsg(db));
		return NULL;
	}

	sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, token, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, platform, -1, SQLITE_TRANSIENT);
	if (name)
		sqlite3_bind_text(stmt, 4, name, -1, SQLITE_TRANSIENT);
	else
		sqlite3_bind_null(stmt, 4);
	sqlite3_bind_text(stmt, 5, secret, -1, SQLITE_TRANSIENT);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		fprintf(stderr, "db: insert error: %s\n", sqlite3_errmsg(db));
		return NULL;
	}

	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "id", id);
	cJSON_AddStringToObject(obj, "token", token);
	cJSON_AddStringToObject(obj, "platform", platform);
	if (name)
		cJSON_AddStringToObject(obj, "name", name);
	cJSON_AddStringToObject(obj, "secret", secret);
	return obj;
}

int db_unregister(sqlite3 *db, const char *id)
{
	const char *sql = "DELETE FROM devices WHERE id = ?;";
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return -1;

	sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE)
		return -1;

	return sqlite3_changes(db) > 0 ? 0 : -1;
}

static cJSON *row_to_json(sqlite3_stmt *stmt)
{
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "id",
		(const char *)sqlite3_column_text(stmt, 0));
	cJSON_AddStringToObject(obj, "token",
		(const char *)sqlite3_column_text(stmt, 1));
	cJSON_AddStringToObject(obj, "platform",
		(const char *)sqlite3_column_text(stmt, 2));

	const char *name = (const char *)sqlite3_column_text(stmt, 3);
	if (name)
		cJSON_AddStringToObject(obj, "name", name);

	cJSON_AddStringToObject(obj, "secret",
		(const char *)sqlite3_column_text(stmt, 4));
	cJSON_AddNumberToObject(obj, "created_at",
		sqlite3_column_int64(stmt, 5));
	return obj;
}

cJSON *db_list(sqlite3 *db)
{
	const char *sql = "SELECT id, token, platform, name, secret, "
			  "created_at FROM devices ORDER BY created_at;";
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return NULL;

	cJSON *arr = cJSON_CreateArray();
	while (sqlite3_step(stmt) == SQLITE_ROW)
		cJSON_AddItemToArray(arr, row_to_json(stmt));

	sqlite3_finalize(stmt);
	return arr;
}

cJSON *db_get(sqlite3 *db, const char *id)
{
	const char *sql = "SELECT id, token, platform, name, secret, "
			  "created_at FROM devices WHERE id = ?;";
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return NULL;

	sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);

	cJSON *obj = NULL;
	if (sqlite3_step(stmt) == SQLITE_ROW)
		obj = row_to_json(stmt);

	sqlite3_finalize(stmt);
	return obj;
}
