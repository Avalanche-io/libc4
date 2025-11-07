#include "../include/c4.h"
#include "c4_internal.h"
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

c4_digest_t* c4id_digest_new(const void* b, size_t length) {
	if (b == NULL || length > 64) {
		return NULL;
	}

	c4_digest_t* digest = malloc(sizeof(c4_digest_t));
	if (digest == NULL) {
		return NULL;
	}

	/* Initialize with zeros */
	memset(digest->data, 0, 64);

	/* Copy data to the end of the buffer (right-aligned) */
	if (length > 0) {
		memcpy(digest->data + (64 - length), b, length);
	}

	return digest;
}

static int compare(const c4_digest_t* left, const c4_digest_t* right) {
	for (int i = 0; i < 64; i++) {
		if (left->data[i] > right->data[i]) {
			return 1;
		} else if (left->data[i] < right->data[i]) {
			return -1;
		}
	}
	return 0;
}

c4_digest_t* c4id_digest_sum(const c4_digest_t* left, const c4_digest_t* right) {
	if (left == NULL || right == NULL) {
		return NULL;
	}

	const c4_digest_t* l = left;
	const c4_digest_t* r = right;

	int cmp = compare(l, r);

	/* If identical, return a copy of left */
	if (cmp == 0) {
		c4_digest_t* result = malloc(sizeof(c4_digest_t));
		if (result == NULL) {
			return NULL;
		}
		memcpy(result->data, l->data, 64);
		return result;
	}

	/* Swap if left > right to ensure smaller comes first */
	if (cmp > 0) {
		const c4_digest_t* tmp = l;
		l = r;
		r = tmp;
	}

	/* Hash the concatenation of the two digests */
	EVP_MD_CTX* ctx = EVP_MD_CTX_create();
	if (ctx == NULL) {
		return NULL;
	}

	EVP_DigestInit_ex(ctx, EVP_sha512(), NULL);
	EVP_DigestUpdate(ctx, l->data, 64);
	EVP_DigestUpdate(ctx, r->data, 64);

	byte md[64];
	unsigned int md_len;
	EVP_DigestFinal_ex(ctx, md, &md_len);
	EVP_MD_CTX_destroy(ctx);

	c4_digest_t* result = c4id_digest_new(md, 64);
	return result;
}

c4_id_t* c4id_digest_to_id(const c4_digest_t* digest) {
	if (digest == NULL) {
		return NULL;
	}

	c4_id_t* id = malloc(sizeof(c4_id_t));
	if (id == NULL) {
		return NULL;
	}

	big_int_t* n = new_big_int(512);
	if (n == NULL) {
		free(id);
		return NULL;
	}

	big_int_set_bytes(n, (unsigned char*)digest->data, 64);
	memcpy(&id->bigint, n, sizeof(big_int_t));
	free(n);

	return id;
}

int c4id_digest_cmp(const c4_digest_t* a, const c4_digest_t* b) {
	if (a == NULL && b == NULL) {
		return 0;
	}
	if (a == NULL) {
		return -1;
	}
	if (b == NULL) {
		return 1;
	}

	return compare(a, b);
}

void c4id_digest_free(c4_digest_t* digest) {
	if (digest != NULL) {
		free(digest);
	}
}

/* Legacy function name for compatibility */
c4_id_t* c4id_digest_id(c4_digest_t* digest) {
	return c4id_digest_to_id(digest);
}
