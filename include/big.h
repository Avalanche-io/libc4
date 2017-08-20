/*
 * big implements provides big integer convenience functions.
 * These functions are only used internally by the c4 id library.
 *
*/

#ifndef __BIG_H__

#include <stdlib.h>
#include <gmp.h>

typedef mpz_t big_int_t;

big_int_t* new_big_int(int size );

void release_big_int(big_int_t *n);

void big_int_set(big_int_t *n, big_int_t *m);

void big_int_mul(big_int_t *q, big_int_t *n, int d);

void big_int_add(big_int_t *q, big_int_t *n, int d);

// DivMod divides `n` by `d`, setting `q` to the quotient and returns the modulo.
int big_int_div_mod(big_int_t *q, big_int_t *n, int d);

void big_int_set_bytes(big_int_t *n, unsigned char *data, int size );
int big_int_get_bytes(big_int_t *n, unsigned char *data, int size );

int big_int_cmp(big_int_t *a, big_int_t *b);

#define __BIG_H__
#endif
