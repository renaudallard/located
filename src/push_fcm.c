#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>

#include "push_fcm.h"
#include "jwt.h"
#include "cJSON.h"

/* Read file into malloc'd buffer. Returns NULL on error. */
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

/* Curl write callback: discard response body. */
static size_t discard_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
	(void)ptr;
	(void)data;
	return size * nmemb;
}

/*
 * Get an OAuth2 access token using the service account JSON key.
 * Returns a malloc'd token string, or NULL on error.
 */
static char *get_access_token(const config_t *cfg)
{
	char *sa_json = read_file(cfg->fcm_service_account_path);
	if (!sa_json)
		return NULL;

	cJSON *sa = cJSON_Parse(sa_json);
	free(sa_json);
	if (!sa)
		return NULL;

	cJSON *email_j = cJSON_GetObjectItemCaseSensitive(sa, "client_email");
	cJSON *key_j = cJSON_GetObjectItemCaseSensitive(sa, "private_key");
	if (!cJSON_IsString(email_j) || !cJSON_IsString(key_j)) {
		cJSON_Delete(sa);
		return NULL;
	}

	time_t now = time(NULL);
	char header[64];
	char payload[512];

	snprintf(header, sizeof(header),
		 "{\"alg\":\"RS256\",\"typ\":\"JWT\"}");
	snprintf(payload, sizeof(payload),
		 "{\"iss\":\"%s\","
		 "\"scope\":\"https://www.googleapis.com/auth/firebase.messaging\","
		 "\"aud\":\"https://oauth2.googleapis.com/token\","
		 "\"iat\":%ld,\"exp\":%ld}",
		 email_j->valuestring, (long)now, (long)(now + 3600));

	char *jwt = jwt_create_rs256(header, payload, key_j->valuestring);
	cJSON_Delete(sa);
	if (!jwt)
		return NULL;

	/* Exchange JWT for access token. */
	CURL *curl = curl_easy_init();
	if (!curl) {
		free(jwt);
		return NULL;
	}

	char post_fields[4096];
	snprintf(post_fields, sizeof(post_fields),
		 "grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Ajwt-bearer"
		 "&assertion=%s", jwt);
	free(jwt);

	/* Capture response. */
	char *resp_buf = NULL;
	size_t resp_size = 0;
	FILE *resp_stream = open_memstream(&resp_buf, &resp_size);

	curl_easy_setopt(curl, CURLOPT_URL,
			 "https://oauth2.googleapis.com/token");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp_stream);

	CURLcode res = curl_easy_perform(curl);
	fclose(resp_stream);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		free(resp_buf);
		return NULL;
	}

	cJSON *resp = cJSON_Parse(resp_buf);
	free(resp_buf);
	if (!resp)
		return NULL;

	cJSON *at = cJSON_GetObjectItemCaseSensitive(resp, "access_token");
	char *token = NULL;
	if (cJSON_IsString(at))
		token = strdup(at->valuestring);

	cJSON_Delete(resp);
	return token;
}

int push_fcm_send(const config_t *cfg, const char *token,
		  const char *request_id, const char *server_url,
		  const char *device_id)
{
	if (!cfg->fcm_project_id || !cfg->fcm_service_account_path) {
		fprintf(stderr, "fcm: missing project_id or service_account_key_path\n");
		return -1;
	}

	char *access_token = get_access_token(cfg);
	if (!access_token) {
		fprintf(stderr, "fcm: failed to get access token\n");
		return -1;
	}

	/* Build FCM v1 message. */
	cJSON *root = cJSON_CreateObject();
	cJSON *message = cJSON_AddObjectToObject(root, "message");
	cJSON_AddStringToObject(message, "token", token);

	cJSON *data = cJSON_AddObjectToObject(message, "data");
	cJSON_AddStringToObject(data, "request_id", request_id);
	cJSON_AddStringToObject(data, "server_url", server_url);
	cJSON_AddStringToObject(data, "device_id", device_id);

	cJSON *android = cJSON_AddObjectToObject(message, "android");
	cJSON_AddStringToObject(android, "priority", "high");

	char *body = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);

	/* Send to FCM. */
	char url[256];
	snprintf(url, sizeof(url),
		 "https://fcm.googleapis.com/v1/projects/%s/messages:send",
		 cfg->fcm_project_id);

	char auth_header[1024];
	snprintf(auth_header, sizeof(auth_header),
		 "Authorization: Bearer %s", access_token);
	free(access_token);

	CURL *curl = curl_easy_init();
	if (!curl) {
		free(body);
		return -1;
	}

	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, auth_header);
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, url);
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
		fprintf(stderr, "fcm: send failed (curl=%d, http=%ld)\n",
			res, http_code);
		return -1;
	}

	return 0;
}
