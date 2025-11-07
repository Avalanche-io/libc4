#include "../include/c4.h"
#include "c4_internal.h"
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

const char* charset = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const unsigned long base = 58;

unsigned char lut[256];

void c4_init(void) {
	for (int i = 0; i < 256; i++) {
		lut[i] = (byte)255;
	}

	for (int i = 0; i < base; i++) {
		lut[(unsigned char)charset[i]] = (byte)i;
	}
}

const char* c4_error_string(c4_error_t err) {
	switch (err) {
		case C4_OK:
			return "No error";
		case C4_ERROR_INVALID_LENGTH:
			return "Invalid length";
		case C4_ERROR_INVALID_CHAR:
			return "Invalid character";
		case C4_ERROR_ALLOCATION:
			return "Memory allocation failed";
		case C4_ERROR_NULL_POINTER:
			return "NULL pointer argument";
		default:
			return "Unknown error";
	}
}

c4_id_t* c4id_parse(const char* src) {
	if (src == NULL) {
		return NULL;
	}

	if (strlen(src) != 90) {
		return NULL;
	}

	if (src[0] != 'c' || src[1] != '4') {
		return NULL;
	}

	c4_id_t* id = malloc(sizeof(c4_id_t));
	if (id == NULL) {
		return NULL;
	}

	big_int_t* n = new_big_int(0);
	if (n == NULL) {
		free(id);
		return NULL;
	}

	for (int i = 2; i < 90; i++) {
		byte b = lut[(unsigned char)src[i]];
		if (b == 0xFF) {
			release_big_int(n);
			free(id);
			return NULL;
		}
		big_int_mul(n, n, base);
		big_int_add(n, n, b);
	}

	memcpy(&id->bigint, n, sizeof(big_int_t));
	free(n);
	return id;
}

char* c4id_string(const c4_id_t* id) {
	if (id == NULL) {
		return NULL;
	}

	char* encoded = malloc(91);
	if (encoded == NULL) {
		return NULL;
	}

	big_int_t* n = new_big_int(512);
	big_int_t* q = new_big_int(0);
	big_int_t* zero = new_big_int(0);

	big_int_set(n, (big_int_t*)&id->bigint);

	for (int i = 89; i > 1; i--) {
		if (big_int_cmp(n, zero) == 0) {
			encoded[i] = '1';
			continue;
		}
		int mod = big_int_div_mod(q, n, base);
		big_int_set(n, q);
		encoded[i] = charset[mod];
	}

	encoded[90] = 0;
	encoded[0] = 'c';
	encoded[1] = '4';

	release_big_int(n);
	release_big_int(q);
	release_big_int(zero);

	return encoded;
}

int c4id_cmp(const c4_id_t* a, const c4_id_t* b) {
	if (a == NULL && b == NULL) {
		return 0;
	}
	if (a == NULL) {
		return -1;
	}
	if (b == NULL) {
		return 1;
	}

	return big_int_cmp((big_int_t*)&a->bigint, (big_int_t*)&b->bigint);
}

void c4id_free(c4_id_t* id) {
	if (id != NULL) {
		mpz_clear(id->bigint);
		free(id);
	}
}

/* Legacy function name for compatibility */
void c4id_release(c4_id_t* id) {
	c4id_free(id);
}
