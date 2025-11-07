#include "../include/c4.h"
#include "c4_internal.h"
#include <gmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

const char* charset = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const unsigned long base = 58;

unsigned char lut[256];

/* Error handling state */
static _Thread_local c4_error_info_t last_error = {C4_OK, NULL, NULL, -1, {0}};
static _Thread_local c4_error_callback_t error_callback = NULL;
static _Thread_local void* error_callback_data = NULL;

/* Internal function to set error details */
static void set_error(c4_error_t code, const char* function, int position, const char* detail_fmt, ...) {
    last_error.code = code;
    last_error.message = c4_error_string(code);
    last_error.function = function;
    last_error.position = position;

    if (detail_fmt != NULL) {
        va_list args;
        va_start(args, detail_fmt);
        vsnprintf(last_error.detail, sizeof(last_error.detail), detail_fmt, args);
        va_end(args);
    } else {
        last_error.detail[0] = '\0';
    }

    /* Call error callback if registered */
    if (error_callback != NULL) {
        error_callback(&last_error, error_callback_data);
    }
}

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

const c4_error_info_t* c4_get_last_error(void) {
	return &last_error;
}

void c4_clear_error(void) {
	last_error.code = C4_OK;
	last_error.message = NULL;
	last_error.function = NULL;
	last_error.position = -1;
	last_error.detail[0] = '\0';
}

void c4_set_error_callback(c4_error_callback_t callback, void* user_data) {
	error_callback = callback;
	error_callback_data = user_data;
}

c4_id_t* c4id_parse(const char* src) {
	c4_clear_error();

	if (src == NULL) {
		set_error(C4_ERROR_NULL_POINTER, "c4id_parse", -1, "Input string is NULL");
		return NULL;
	}

	size_t len = strlen(src);
	if (len != 90) {
		set_error(C4_ERROR_INVALID_LENGTH, "c4id_parse", -1,
		          "Expected 90 characters, got %zu", len);
		return NULL;
	}

	if (src[0] != 'c' || src[1] != '4') {
		set_error(C4_ERROR_INVALID_CHAR, "c4id_parse", 0,
		          "ID must start with 'c4', got '%c%c'", src[0], src[1]);
		return NULL;
	}

	c4_id_t* id = malloc(sizeof(c4_id_t));
	if (id == NULL) {
		set_error(C4_ERROR_ALLOCATION, "c4id_parse", -1,
		          "Failed to allocate %zu bytes for ID", sizeof(c4_id_t));
		return NULL;
	}

	big_int_t* n = new_big_int(0);
	if (n == NULL) {
		free(id);
		set_error(C4_ERROR_ALLOCATION, "c4id_parse", -1,
		          "Failed to allocate big integer");
		return NULL;
	}

	for (int i = 2; i < 90; i++) {
		byte b = lut[(unsigned char)src[i]];
		if (b == 0xFF) {
			release_big_int(n);
			free(id);
			set_error(C4_ERROR_INVALID_CHAR, "c4id_parse", i,
			          "Invalid base58 character '%c' (0x%02X) at position %d",
			          src[i], (unsigned char)src[i], i);
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
	c4_clear_error();

	if (id == NULL) {
		set_error(C4_ERROR_NULL_POINTER, "c4id_string", -1, "ID pointer is NULL");
		return NULL;
	}

	char* encoded = malloc(91);
	if (encoded == NULL) {
		set_error(C4_ERROR_ALLOCATION, "c4id_string", -1,
		          "Failed to allocate 91 bytes for string");
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
