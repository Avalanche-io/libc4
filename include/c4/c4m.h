// SPDX-License-Identifier: Apache-2.0
// C4M Format - C API
// Parser and encoder for .c4m files

#ifndef C4_C4M_H
#define C4_C4M_H

#include "c4.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Entry types
typedef enum {
    C4M_ENTRY_FILE = 0,
    C4M_ENTRY_DIR,
    C4M_ENTRY_SEQUENCE,
} c4m_entry_type_t;

// A single entry in a .c4m manifest
typedef struct c4m_entry c4m_entry_t;

// Access entry fields
c4m_entry_type_t c4m_entry_type(const c4m_entry_t *entry);
const char      *c4m_entry_name(const c4m_entry_t *entry);
const c4_id_t   *c4m_entry_id(const c4m_entry_t *entry);
int64_t          c4m_entry_size(const c4m_entry_t *entry);
// For sequences: frame range
int              c4m_entry_frame_start(const c4m_entry_t *entry);
int              c4m_entry_frame_end(const c4m_entry_t *entry);

// Opaque manifest type
typedef struct c4m_manifest c4m_manifest_t;

// Parse a .c4m file from a buffer
c4m_manifest_t *c4m_parse(const char *data, size_t len, c4_error_t *err);

// Parse a .c4m file from a file path
c4m_manifest_t *c4m_parse_file(const char *path, c4_error_t *err);

// Free a manifest
void c4m_free(c4m_manifest_t *m);

// Manifest accessors
size_t               c4m_entry_count(const c4m_manifest_t *m);
const c4m_entry_t   *c4m_entry_at(const c4m_manifest_t *m, size_t index);
const c4_id_t       *c4m_root_id(const c4m_manifest_t *m);

// Encode a manifest to a string. Caller must free the returned buffer.
char *c4m_encode(const c4m_manifest_t *m, size_t *out_len, c4_error_t *err);

// Diff two manifests. Returns a new manifest containing only entries that
// differ between a and b.
c4m_manifest_t *c4m_diff(const c4m_manifest_t *a, const c4m_manifest_t *b,
                         c4_error_t *err);

#ifdef __cplusplus
}
#endif

#endif // C4_C4M_H
