#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
	int listen_port;
	char *db_path;
	char *api_key;
	char *server_url;		/* public URL for push callbacks */

	/* FCM */
	char *fcm_project_id;
	char *fcm_service_account_path;

	/* APNs */
	char *apns_key_path;
	char *apns_key_id;
	char *apns_team_id;
	char *apns_bundle_id;
	int apns_sandbox;

	int locate_timeout;		/* seconds to wait for device response */
} config_t;

/* Load config from JSON file. Returns 0 on success, -1 on error. */
int config_load(config_t *cfg, const char *path);

/* Set defaults. */
void config_defaults(config_t *cfg);

/* Free allocated strings in config. */
void config_free(config_t *cfg);

#endif
