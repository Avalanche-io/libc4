/*
 * big.c implements provides big integer convenience functions.
 * These functions are only used internally by the c4 id library.
 *
*/

#include <stdlib.h>
#include <stdio.h>
#include <gmp.h>

typedef mpz_t big_int_t;

big_int_t* new_big_int(int size ) {
	big_int_t *n = malloc(sizeof(big_int_t));

	if (size > 0) {
		mpz_init2(*n, size);
		return n;
	}
	mpz_init(*n);
	return n;
}

void release_big_int(big_int_t *n) {
	mpz_clear( *n );
	if (n != NULL) {
		free(n);
	}
}

void big_int_set(big_int_t *n, big_int_t *m) {
	mpz_set(*n, *m);
}

void big_int_mul(big_int_t *q, big_int_t *n, int d) {
	mpz_mul_ui(*q, *n, d);
}

void big_int_add(big_int_t *q, big_int_t *n, int d) {
	mpz_add_ui(*q, *n, (unsigned long) d);
}

// DivMod divides `n` by `d`, setting `q` to the quotient and returns the modulo.
int big_int_div_mod(big_int_t *q, big_int_t *n, int d) {
	return mpz_tdiv_q_ui(*q, *n, d);
}

void big_int_set_bytes(big_int_t *n, unsigned char *data, int size ) {
	// mpz_init2(n, 512);
	// printf("SetBytes \n");
	mpz_import(*n, 1, 1, size, 1, 0, data);
}

int big_int_get_bytes(big_int_t *n, void *data, int size ) {
	size_t count;
 	mpz_export(data, &count, 1, size, 1, 0, *n);
	return (int)count;
}

int big_int_cmp(big_int_t *a, big_int_t *b) {
	return mpz_cmp(*a, *b);
}

