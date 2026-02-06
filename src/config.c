#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cJSON.h"

void config_defaults(config_t *cfg)
{
	cfg->listen_port = 8080;
	cfg->db_path = strdup("./devices.db");
	cfg->api_key = NULL;
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

	cJSON *port = cJSON_GetObjectItemCaseSensitive(root, "listen_port");
	if (cJSON_IsNumber(port))
		cfg->listen_port = port->valueint;

	cJSON *db = cJSON_GetObjectItemCaseSensitive(root, "db_path");
	if (cJSON_IsString(db) && db->valuestring) {
		free(cfg->db_path);
		cfg->db_path = strdup(db->valuestring);
	}

	cJSON *key = cJSON_GetObjectItemCaseSensitive(root, "api_key");
	if (cJSON_IsString(key) && key->valuestring)
		cfg->api_key = strdup(key->valuestring);

	cJSON_Delete(root);
	return 0;
}

void config_free(config_t *cfg)
{
	free(cfg->db_path);
	free(cfg->api_key);
	cfg->db_path = NULL;
	cfg->api_key = NULL;
}
