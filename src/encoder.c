#include "../include/c4.h"
#include "c4_internal.h"
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

c4id_encoder_t* c4id_encoder_new(void) {
	c4id_encoder_t* enc = malloc(sizeof(c4id_encoder_t));
	if (enc == NULL) {
		return NULL;
	}

	enc->ctx = EVP_MD_CTX_create();
	if (enc->ctx == NULL) {
		free(enc);
		return NULL;
	}

	EVP_DigestInit_ex(enc->ctx, EVP_sha512(), NULL);
	return enc;
}

void c4id_encoder_write(c4id_encoder_t* enc, const void* data, size_t size) {
	if (enc == NULL || data == NULL) {
		return;
	}
	EVP_DigestUpdate(enc->ctx, data, size);
}

c4_id_t* c4id_encoder_id(c4id_encoder_t* enc) {
	if (enc == NULL) {
		return NULL;
	}

	byte md[64];
	unsigned int md_len;

	/* Create a copy of the context to avoid finalizing the original */
	EVP_MD_CTX* ctx_copy = EVP_MD_CTX_create();
	if (ctx_copy == NULL) {
		return NULL;
	}

	EVP_MD_CTX_copy_ex(ctx_copy, enc->ctx);
	EVP_DigestFinal_ex(ctx_copy, md, &md_len);
	EVP_MD_CTX_destroy(ctx_copy);

	c4_id_t* id = malloc(sizeof(c4_id_t));
	if (id == NULL) {
		return NULL;
	}

	big_int_t* n = new_big_int(512);
	if (n == NULL) {
		free(id);
		return NULL;
	}

	big_int_set_bytes(n, md, 64);
	memcpy(&id->bigint, n, sizeof(big_int_t));
	free(n);

	return id;
}

c4_digest_t* c4id_encoder_digest(c4id_encoder_t* enc) {
	if (enc == NULL) {
		return NULL;
	}

	c4_digest_t* digest = malloc(sizeof(c4_digest_t));
	if (digest == NULL) {
		return NULL;
	}

	unsigned int md_len;

	/* Create a copy of the context to avoid finalizing the original */
	EVP_MD_CTX* ctx_copy = EVP_MD_CTX_create();
	if (ctx_copy == NULL) {
		free(digest);
		return NULL;
	}

	EVP_MD_CTX_copy_ex(ctx_copy, enc->ctx);
	EVP_DigestFinal_ex(ctx_copy, digest->data, &md_len);
	EVP_MD_CTX_destroy(ctx_copy);

	return digest;
}

void c4id_encoder_reset(c4id_encoder_t* enc) {
	if (enc == NULL || enc->ctx == NULL) {
		return;
	}

	EVP_MD_CTX_destroy(enc->ctx);
	enc->ctx = EVP_MD_CTX_create();
	if (enc->ctx != NULL) {
		EVP_DigestInit_ex(enc->ctx, EVP_sha512(), NULL);
	}
}

void c4id_encoder_free(c4id_encoder_t* enc) {
	if (enc != NULL) {
		if (enc->ctx != NULL) {
			EVP_MD_CTX_destroy(enc->ctx);
		}
		free(enc);
	}
}

/* Legacy function names for compatibility */
c4id_encoder_t* c4id_new_encoder(void) {
	return c4id_encoder_new();
}

void c4id_encoder_update(c4id_encoder_t* enc, unsigned char* data, int size) {
	c4id_encoder_write(enc, data, (size_t)size);
}

c4_id_t* c4id_encoded_id(c4id_encoder_t* enc) {
	return c4id_encoder_id(enc);
}

void c4id_release_encoder(c4id_encoder_t* enc) {
	c4id_encoder_free(enc);
}
