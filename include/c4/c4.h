// SPDX-License-Identifier: Apache-2.0
// C4 ID System - C API
// Reference implementation of SMPTE ST 2114:2017

#ifndef C4_C4_H
#define C4_C4_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// C4 ID constants
#define C4_ID_LEN       90   // Encoded string length (including "c4" prefix)
#define C4_DIGEST_LEN   64   // Raw SHA-512 digest length in bytes

// Error codes
typedef enum {
    C4_OK = 0,
    C4_ERR_INVALID_ID,
    C4_ERR_INVALID_INPUT,
    C4_ERR_IO,
    C4_ERR_ALLOC,
} c4_error_t;

// Opaque ID type (64-byte digest internally)
typedef struct c4_id c4_id_t;

// Allocate/free
c4_id_t *c4_id_new(void);
void     c4_id_free(c4_id_t *id);

// Identify data: compute C4 ID from a buffer
c4_error_t c4_identify(const void *data, size_t len, c4_id_t *out);

// Identify file: compute C4 ID from a file descriptor
c4_error_t c4_identify_fd(int fd, c4_id_t *out);

// Identify file by path (c4m-aware: .c4m extension triggers canonical heuristic)
c4_error_t c4_identify_file(const char *path, c4_id_t *out);

// c4m detection heuristic: returns 1 if data looks like it might be c4m
int c4_looks_like_c4m(const char *data, size_t len);

// c4m-aware identification: if data parses as valid c4m, canonicalize
// before hashing. Otherwise hash raw bytes.
c4_error_t c4_identify_c4m_aware(const void *data, size_t len, c4_id_t *out);

// Encode ID to string. buf must be at least C4_ID_LEN+1 bytes.
c4_error_t c4_id_string(const c4_id_t *id, char *buf, size_t buflen);

// Parse string to ID
c4_error_t c4_id_parse(const char *str, size_t len, c4_id_t *out);

// Compare two IDs. Returns <0, 0, or >0.
int c4_id_compare(const c4_id_t *a, const c4_id_t *b);

// Check if two IDs are equal
int c4_id_equal(const c4_id_t *a, const c4_id_t *b);

// Get raw digest bytes (64 bytes)
const uint8_t *c4_id_digest(const c4_id_t *id);

// Set ID from raw digest bytes (64 bytes)
c4_error_t c4_id_from_digest(const void *digest, size_t len, c4_id_t *out);

// Tree operations: compute the C4 ID of a sorted set of IDs.
// IDs are sorted internally. The result is the ID of the concatenated digests.
c4_error_t c4_tree_id(const c4_id_t **ids, size_t count, c4_id_t *out);

#ifdef __cplusplus
}
#endif

#endif // C4_C4_H
