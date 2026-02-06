#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "config.h"
#include "device_db.h"
#include "http_server.h"
#include "routes.h"

#define VERSION "0.1.0"
#define PROG    "located"

static void usage(void)
{
	fprintf(stderr,
		"Usage: " PROG " <command> [options]\n"
		"\n"
		"Commands:\n"
		"  daemon, -d      Start the HTTP server\n"
		"  devices         List registered devices\n"
		"  locate <id>     Locate a device (Phase 2)\n"
		"  help, -h        Show this help\n"
		"  version, -v     Show version\n"
		"\n"
		"Options:\n"
		"  -c <path>       Config file (default: /etc/located.json)\n");
}

/* Scan argv for -c <path>, return path or NULL. */
static const char *find_config_path(int argc, char **argv)
{
	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], "-c") == 0)
			return argv[i + 1];
	}
	return NULL;
}

static int cmd_daemon(config_t *cfg)
{
	sqlite3 *db;
	if (db_open(&db, cfg->db_path) != 0)
		return 1;

	route_ctx_t ctx = { .db = db, .api_key = cfg->api_key };

	struct MHD_Daemon *d = http_server_start(cfg->listen_port, &ctx);
	if (!d) {
		db_close(db);
		return 1;
	}

	/* Block until SIGINT or SIGTERM. */
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigprocmask(SIG_BLOCK, &set, NULL);

	int sig;
	fprintf(stderr, PROG ": running (pid %d), press Ctrl-C to stop\n",
		getpid());
	sigwait(&set, &sig);
	fprintf(stderr, "\n" PROG ": shutting down (signal %d)\n", sig);

	http_server_stop(d);
	db_close(db);
	return 0;
}

static int cmd_devices(config_t *cfg)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "curl -s http://localhost:%d/api/devices", cfg->listen_port);
	return system(cmd);
}

static int cmd_locate(config_t *cfg, const char *id)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "curl -s -X POST http://localhost:%d/api/locate/%s",
		 cfg->listen_port, id);
	return system(cmd);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 1;
	}

	const char *cmd = argv[1];

	/* Skip -c if it's the first arg to get to the command. */
	if (strcmp(cmd, "-c") == 0) {
		if (argc < 4) {
			usage();
			return 1;
		}
		cmd = argv[3];
	}

	if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0) {
		usage();
		return 0;
	}

	if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-v") == 0) {
		printf(PROG " %s\n", VERSION);
		return 0;
	}

	/* Commands below need config. */
	config_t cfg;
	const char *cfgpath = find_config_path(argc, argv);
	if (cfgpath) {
		if (config_load(&cfg, cfgpath) != 0)
			return 1;
	} else {
		config_defaults(&cfg);
	}

	int ret = 0;

	if (strcmp(cmd, "daemon") == 0 || strcmp(cmd, "-d") == 0) {
		ret = cmd_daemon(&cfg);
	} else if (strcmp(cmd, "devices") == 0) {
		ret = cmd_devices(&cfg);
	} else if (strcmp(cmd, "locate") == 0) {
		/* Find the device id (next non-option arg after "locate"). */
		const char *id = NULL;
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "locate") == 0 && i + 1 < argc) {
				id = argv[i + 1];
				if (strcmp(id, "-c") == 0)
					id = (i + 3 < argc) ? argv[i + 3] : NULL;
				break;
			}
		}
		if (!id) {
			fprintf(stderr, "Usage: " PROG " locate <device-id>\n");
			ret = 1;
		} else {
			ret = cmd_locate(&cfg, id);
		}
	} else {
		fprintf(stderr, PROG ": unknown command '%s'\n", cmd);
		usage();
		ret = 1;
	}

	config_free(&cfg);
	return ret;
}
