#ifndef C4_H
#define C4_H

#include <stddef.h>

/*
 * C4 ID Library
 *
 * Implementation of the SMPTE standard ST 2114:2017 for universally unique
 * and consistent identification.
 *
 * C4 IDs are 90-character base58-encoded SHA-512 hashes that provide
 * consistent identification of data across all observers worldwide without
 * requiring a central registry.
 */

/* Opaque types - implementation details are hidden */
typedef struct c4_id_s c4_id_t;
typedef struct c4id_encoder_s c4id_encoder_t;
typedef struct c4_digest_s c4_digest_t;

/* Error codes */
typedef enum {
    C4_OK = 0,
    C4_ERROR_INVALID_LENGTH,
    C4_ERROR_INVALID_CHAR,
    C4_ERROR_ALLOCATION,
    C4_ERROR_NULL_POINTER
} c4_error_t;

/* Get human-readable error message */
const char* c4_error_string(c4_error_t err);

/* Library initialization - must be called before using any other functions */
void c4_init(void);

/* ID operations */

/* Parse a C4 ID from a 90-character string. Returns NULL on error. */
c4_id_t* c4id_parse(const char* src);

/* Convert a C4 ID to its 90-character string representation.
 * Caller must free the returned string with free(). */
char* c4id_string(const c4_id_t* id);

/* Compare two IDs. Returns -1 if a < b, 0 if a == b, 1 if a > b. */
int c4id_cmp(const c4_id_t* a, const c4_id_t* b);

/* Free a C4 ID */
void c4id_free(c4_id_t* id);

/* Encoder operations - for streaming hash computation */

/* Create a new encoder for generating IDs from data */
c4id_encoder_t* c4id_encoder_new(void);

/* Write data to the encoder */
void c4id_encoder_write(c4id_encoder_t* enc, const void* data, size_t size);

/* Get the final ID for all data written to the encoder */
c4_id_t* c4id_encoder_id(c4id_encoder_t* enc);

/* Get the digest without finalizing (can continue writing after) */
c4_digest_t* c4id_encoder_digest(c4id_encoder_t* enc);

/* Reset the encoder to identify a new block of data */
void c4id_encoder_reset(c4id_encoder_t* enc);

/* Free an encoder */
void c4id_encoder_free(c4id_encoder_t* enc);

/* Digest operations - 64-byte SHA-512 digests */

/* Create a digest from bytes, padding to 64 bytes if needed.
 * Returns NULL if length > 64. */
c4_digest_t* c4id_digest_new(const void* data, size_t length);

/* Combine two digests using C4's ordered concatenation and hashing.
 * Returns a new digest (caller must free). */
c4_digest_t* c4id_digest_sum(const c4_digest_t* left, const c4_digest_t* right);

/* Convert a digest to a C4 ID (does not rehash) */
c4_id_t* c4id_digest_to_id(const c4_digest_t* digest);

/* Compare two digests lexicographically. Returns -1, 0, or 1. */
int c4id_digest_cmp(const c4_digest_t* a, const c4_digest_t* b);

/* Free a digest */
void c4id_digest_free(c4_digest_t* digest);

#endif /* C4_H */
