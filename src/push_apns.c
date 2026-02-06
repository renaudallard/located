#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>

#include "push_apns.h"
#include "jwt.h"
#include "cJSON.h"

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		perror(path);
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buf = malloc(len + 1);
	if (!buf || fread(buf, 1, len, f) != (size_t)len) {
		free(buf);
		fclose(f);
		return NULL;
	}
	buf[len] = '\0';
	fclose(f);
	return buf;
}

static size_t discard_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
	(void)ptr;
	(void)data;
	return size * nmemb;
}

static char *create_apns_jwt(const config_t *cfg)
{
	char *key_pem = read_file(cfg->apns_key_path);
	if (!key_pem)
		return NULL;

	time_t now = time(NULL);
	char header[128];
	char payload[128];

	snprintf(header, sizeof(header),
		 "{\"alg\":\"ES256\",\"kid\":\"%s\"}", cfg->apns_key_id);
	snprintf(payload, sizeof(payload),
		 "{\"iss\":\"%s\",\"iat\":%ld}", cfg->apns_team_id, (long)now);

	char *jwt = jwt_create_es256(header, payload, key_pem);
	free(key_pem);
	return jwt;
}

int push_apns_send(const config_t *cfg, const char *token,
		   const char *request_id, const char *server_url,
		   const char *device_id)
{
	if (!cfg->apns_key_path || !cfg->apns_key_id ||
	    !cfg->apns_team_id || !cfg->apns_bundle_id) {
		fprintf(stderr, "apns: missing config fields\n");
		return -1;
	}

	char *jwt = create_apns_jwt(cfg);
	if (!jwt) {
		fprintf(stderr, "apns: failed to create JWT\n");
		return -1;
	}

	/* Build APNs payload. */
	cJSON *root = cJSON_CreateObject();
	cJSON *aps = cJSON_AddObjectToObject(root, "aps");
	cJSON_AddNumberToObject(aps, "content-available", 1);
	cJSON_AddStringToObject(root, "request_id", request_id);
	cJSON_AddStringToObject(root, "server_url", server_url);
	cJSON_AddStringToObject(root, "device_id", device_id);

	char *body = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);

	/* Build URL. */
	const char *host = cfg->apns_sandbox
		? "api.sandbox.push.apple.com"
		: "api.push.apple.com";
	char url[512];
	snprintf(url, sizeof(url), "https://%s/3/device/%s", host, token);

	char auth_header[2048];
	snprintf(auth_header, sizeof(auth_header),
		 "authorization: bearer %s", jwt);
	free(jwt);

	char topic_header[256];
	snprintf(topic_header, sizeof(topic_header),
		 "apns-topic: %s", cfg->apns_bundle_id);

	CURL *curl = curl_easy_init();
	if (!curl) {
		free(body);
		return -1;
	}

	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, auth_header);
	headers = curl_slist_append(headers, topic_header);
	headers = curl_slist_append(headers, "apns-push-type: background");
	headers = curl_slist_append(headers, "apns-priority: 5");
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_cb);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	free(body);

	if (res != CURLE_OK || http_code != 200) {
		fprintf(stderr, "apns: send failed (curl=%d, http=%ld)\n",
			res, http_code);
		return -1;
	}

	return 0;
}
