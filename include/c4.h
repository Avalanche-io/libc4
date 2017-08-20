#ifndef __C4_H__

#include <openssl/evp.h>
#include <gmp.h>

typedef mpz_t c4_id_t;

// init initializes the id library
void c4_init();

// c4id_parse parses a C4 ID from a NULL terminated c string.
c4_id_t* c4id_parse(const char*);

// c4id_string returns the ID as a NULL terminated c string.
// It's the callers responsibility to release the string allocation with free().
char* c4id_string(c4_id_t*);

// c4id_release frees all allocations for the given c4 id.
void c4id_release(c4_id_t*);

// c4id_encoder_t generates an ID for a contiguous bock of data.
typedef struct {
	// SHA512_CTX *c;
	EVP_MD_CTX *c;
} c4id_encoder_t;

// c4id_new_encoder makes a new Encoder.
c4id_encoder_t* c4id_new_encoder();

// c4id_release_encoder frees all allocations for the given Encoder.
void c4id_release_encoder(c4id_encoder_t*);

//  EncoderUpdate writes `size` bytes from `data` to the encoder `e`
void c4id_encoder_update(c4id_encoder_t*, unsigned char*, int);

// ID returns the ID for the bytes written to the encoder.
c4_id_t* c4id_encoded_id(c4id_encoder_t*);

typedef unsigned char* c4_digest_t;

c4_digest_t c4id_new_digest(unsigned char *b, int length);

c4_digest_t c4id_digest_sum(c4_digest_t l, c4_digest_t r);

c4_id_t* c4id_digest_id(c4_digest_t d);

#define __C4_H__
#endif

