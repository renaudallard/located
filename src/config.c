#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cJSON.h"

static char *jstrdup(cJSON *obj, const char *key)
{
	cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
	if (cJSON_IsString(v) && v->valuestring)
		return strdup(v->valuestring);
	return NULL;
}

void config_defaults(config_t *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->listen_port = 8080;
	cfg->db_path = strdup("./devices.db");
	cfg->locate_timeout = 30;
	cfg->apns_sandbox = 1;
}

int config_load(config_t *cfg, const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		perror(path);
		return -1;
	}

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buf = malloc(len + 1);
	if (!buf) {
		fclose(f);
		return -1;
	}
	if (fread(buf, 1, len, f) != (size_t)len) {
		free(buf);
		fclose(f);
		return -1;
	}
	buf[len] = '\0';
	fclose(f);

	cJSON *root = cJSON_Parse(buf);
	free(buf);
	if (!root) {
		fprintf(stderr, "config: JSON parse error\n");
		return -1;
	}

	config_defaults(cfg);

	cJSON *v;

	v = cJSON_GetObjectItemCaseSensitive(root, "listen_port");
	if (cJSON_IsNumber(v))
		cfg->listen_port = v->valueint;

	v = cJSON_GetObjectItemCaseSensitive(root, "locate_timeout");
	if (cJSON_IsNumber(v))
		cfg->locate_timeout = v->valueint;

	v = cJSON_GetObjectItemCaseSensitive(root, "db_path");
	if (cJSON_IsString(v) && v->valuestring) {
		free(cfg->db_path);
		cfg->db_path = strdup(v->valuestring);
	}

	cfg->api_key = jstrdup(root, "api_key");
	cfg->server_url = jstrdup(root, "server_url");

	/* FCM */
	cJSON *fcm = cJSON_GetObjectItemCaseSensitive(root, "fcm");
	if (fcm) {
		cfg->fcm_project_id = jstrdup(fcm, "project_id");
		cfg->fcm_service_account_path = jstrdup(fcm, "service_account_key_path");
	}

	/* APNs */
	cJSON *apns = cJSON_GetObjectItemCaseSensitive(root, "apns");
	if (apns) {
		cfg->apns_key_path = jstrdup(apns, "key_path");
		cfg->apns_key_id = jstrdup(apns, "key_id");
		cfg->apns_team_id = jstrdup(apns, "team_id");
		cfg->apns_bundle_id = jstrdup(apns, "bundle_id");

		v = cJSON_GetObjectItemCaseSensitive(apns, "use_sandbox");
		if (cJSON_IsBool(v))
			cfg->apns_sandbox = cJSON_IsTrue(v);
	}

	cJSON_Delete(root);
	return 0;
}

void config_free(config_t *cfg)
{
	free(cfg->db_path);
	free(cfg->api_key);
	free(cfg->server_url);
	free(cfg->fcm_project_id);
	free(cfg->fcm_service_account_path);
	free(cfg->apns_key_path);
	free(cfg->apns_key_id);
	free(cfg->apns_team_id);
	free(cfg->apns_bundle_id);
	memset(cfg, 0, sizeof(*cfg));
}
