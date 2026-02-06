#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ecdsa.h>

#include "jwt.h"

static char *base64url_encode(const unsigned char *data, size_t len)
{
	/* Base64 output size: 4 * ceil(len/3) + 1 */
	size_t out_max = 4 * ((len + 2) / 3) + 1;
	char *out = malloc(out_max);
	if (!out)
		return NULL;

	static const char tbl[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t i, j = 0;
	for (i = 0; i + 2 < len; i += 3) {
		unsigned int v = ((unsigned int)data[i] << 16) |
				 ((unsigned int)data[i + 1] << 8) |
				 data[i + 2];
		out[j++] = tbl[(v >> 18) & 0x3F];
		out[j++] = tbl[(v >> 12) & 0x3F];
		out[j++] = tbl[(v >> 6) & 0x3F];
		out[j++] = tbl[v & 0x3F];
	}
	if (i < len) {
		unsigned int v = (unsigned int)data[i] << 16;
		if (i + 1 < len)
			v |= (unsigned int)data[i + 1] << 8;
		out[j++] = tbl[(v >> 18) & 0x3F];
		out[j++] = tbl[(v >> 12) & 0x3F];
		if (i + 1 < len)
			out[j++] = tbl[(v >> 6) & 0x3F];
	}
	out[j] = '\0';

	/* Convert to URL-safe: + -> -, / -> _ (no padding) */
	for (size_t k = 0; k < j; k++) {
		if (out[k] == '+') out[k] = '-';
		else if (out[k] == '/') out[k] = '_';
	}
	return out;
}

static EVP_PKEY *load_pem_key(const char *pem_key)
{
	BIO *bio = BIO_new_mem_buf(pem_key, -1);
	if (!bio)
		return NULL;

	EVP_PKEY *key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
	BIO_free(bio);
	return key;
}

static char *jwt_build(const char *header_json, const char *payload_json,
		       EVP_PKEY *pkey, const EVP_MD *md)
{
	char *hdr_b64 = base64url_encode((const unsigned char *)header_json,
					 strlen(header_json));
	char *pay_b64 = base64url_encode((const unsigned char *)payload_json,
					 strlen(payload_json));
	if (!hdr_b64 || !pay_b64) {
		free(hdr_b64);
		free(pay_b64);
		return NULL;
	}

	/* header.payload */
	size_t msg_len = strlen(hdr_b64) + 1 + strlen(pay_b64);
	char *msg = malloc(msg_len + 1);
	snprintf(msg, msg_len + 1, "%s.%s", hdr_b64, pay_b64);

	/* Sign */
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	size_t sig_len = 0;
	unsigned char *sig = NULL;
	char *result = NULL;

	if (EVP_DigestSignInit(ctx, NULL, md, NULL, pkey) != 1)
		goto out;
	if (EVP_DigestSignUpdate(ctx, msg, msg_len) != 1)
		goto out;
	if (EVP_DigestSignFinal(ctx, NULL, &sig_len) != 1)
		goto out;

	sig = malloc(sig_len);
	if (EVP_DigestSignFinal(ctx, sig, &sig_len) != 1)
		goto out;

	/*
	 * For ECDSA (ES256), OpenSSL returns DER-encoded signature.
	 * JWT needs raw R || S (32 bytes each = 64 bytes total).
	 */
	unsigned char *jwt_sig = sig;
	size_t jwt_sig_len = sig_len;
	unsigned char rs_buf[64];

	if (EVP_PKEY_id(pkey) == EVP_PKEY_EC) {
		const unsigned char *p = sig;
		ECDSA_SIG *ec_sig = d2i_ECDSA_SIG(NULL, &p, (long)sig_len);
		if (!ec_sig)
			goto out;

		const BIGNUM *r = NULL, *s = NULL;
		ECDSA_SIG_get0(ec_sig, &r, &s);

		memset(rs_buf, 0, 64);
		BN_bn2bin(r, rs_buf + (32 - BN_num_bytes(r)));
		BN_bn2bin(s, rs_buf + 32 + (32 - BN_num_bytes(s)));
		ECDSA_SIG_free(ec_sig);

		jwt_sig = rs_buf;
		jwt_sig_len = 64;
	}

	char *sig_b64 = base64url_encode(jwt_sig, jwt_sig_len);
	if (!sig_b64)
		goto out;

	/* header.payload.signature */
	size_t total = msg_len + 1 + strlen(sig_b64) + 1;
	result = malloc(total);
	snprintf(result, total, "%s.%s", msg, sig_b64);
	free(sig_b64);

out:
	EVP_MD_CTX_free(ctx);
	free(sig);
	free(msg);
	free(hdr_b64);
	free(pay_b64);
	return result;
}

char *jwt_create_rs256(const char *header_json, const char *payload_json,
		       const char *pem_key)
{
	EVP_PKEY *key = load_pem_key(pem_key);
	if (!key) {
		fprintf(stderr, "jwt: failed to load RSA key\n");
		return NULL;
	}

	char *token = jwt_build(header_json, payload_json, key, EVP_sha256());
	EVP_PKEY_free(key);
	return token;
}

char *jwt_create_es256(const char *header_json, const char *payload_json,
		       const char *pem_key)
{
	EVP_PKEY *key = load_pem_key(pem_key);
	if (!key) {
		fprintf(stderr, "jwt: failed to load EC key\n");
		return NULL;
	}

	char *token = jwt_build(header_json, payload_json, key, EVP_sha256());
	EVP_PKEY_free(key);
	return token;
}
