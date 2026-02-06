#ifndef JWT_H
#define JWT_H

/*
 * Create an RS256-signed JWT (for Google OAuth2 / FCM).
 * pem_key: PEM-encoded RSA private key.
 * Returns a malloc'd JWT string, or NULL on error. Caller must free.
 */
char *jwt_create_rs256(const char *header_json, const char *payload_json,
		       const char *pem_key);

/*
 * Create an ES256-signed JWT (for APNs).
 * pem_key: PEM-encoded EC P-256 private key (.p8 file contents).
 * Returns a malloc'd JWT string, or NULL on error. Caller must free.
 */
char *jwt_create_es256(const char *header_json, const char *payload_json,
		       const char *pem_key);

#endif
