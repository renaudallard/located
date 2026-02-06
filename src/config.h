#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
	int listen_port;
	char *db_path;
	char *api_key;
} config_t;

/* Load config from JSON file. Returns 0 on success, -1 on error. */
int config_load(config_t *cfg, const char *path);

/* Set defaults (port 8080, db_path "./devices.db", api_key NULL). */
void config_defaults(config_t *cfg);

/* Free allocated strings in config. */
void config_free(config_t *cfg);

#endif
