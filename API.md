# libc4 API Reference

Complete API documentation for libc4, the C implementation of the C4 ID specification (SMPTE ST 2114:2017).

## Table of Contents

- [Overview](#overview)
- [Getting Started](#getting-started)
- [Types](#types)
- [Error Handling](#error-handling)
- [API Functions](#api-functions)
  - [Initialization](#initialization)
  - [ID Operations](#id-operations)
  - [Encoder Operations](#encoder-operations)
  - [Digest Operations](#digest-operations)
- [Examples](#examples)
- [Error Handling Examples](#error-handling-examples)
- [Legacy API](#legacy-api)

## Overview

C4 IDs are 90-character base58-encoded SHA-512 hashes that provide consistent identification of data across all observers worldwide without requiring a central registry. This library provides a minimal, efficient C implementation.

## Getting Started

### Installation

```bash
# Using Make
make
sudo cp lib/libc4.a /usr/local/lib/
sudo cp include/c4.h /usr/local/include/

# Using CMake
mkdir build && cd build
cmake ..
make
sudo make install
```

### Basic Usage

```c
#include <c4.h>
#include <stdio.h>

int main(void) {
    /* Initialize library (call once at startup) */
    c4_init();

    /* Create encoder */
    c4id_encoder_t* enc = c4id_encoder_new();

    /* Hash some data */
    const char* data = "Hello, World!";
    c4id_encoder_write(enc, data, strlen(data));

    /* Get the C4 ID */
    c4_id_t* id = c4id_encoder_id(enc);

    /* Convert to string */
    char* str = c4id_string(id);
    printf("C4 ID: %s\n", str);

    /* Cleanup */
    free(str);
    c4id_free(id);
    c4id_encoder_free(enc);

    return 0;
}
```

Compile with:
```bash
gcc -o example example.c -lc4 -lgmp -lssl -lcrypto
```

## Types

### Opaque Types

All types are opaque pointers, hiding implementation details:

- `c4_id_t` - A C4 ID (90-character base58-encoded identifier)
- `c4id_encoder_t` - Streaming encoder for generating IDs from data
- `c4_digest_t` - 64-byte SHA-512 digest

### Error Types

```c
typedef enum {
    C4_OK = 0,                    /* No error */
    C4_ERROR_INVALID_LENGTH,      /* Invalid string length */
    C4_ERROR_INVALID_CHAR,        /* Invalid character in input */
    C4_ERROR_ALLOCATION,          /* Memory allocation failed */
    C4_ERROR_NULL_POINTER         /* NULL pointer argument */
} c4_error_t;

typedef struct {
    c4_error_t code;              /* Error code */
    const char* message;          /* Human-readable message */
    const char* function;         /* Function where error occurred */
    int position;                 /* Position in input (-1 if N/A) */
    char detail[256];             /* Additional error details */
} c4_error_info_t;

typedef void (*c4_error_callback_t)(const c4_error_info_t* error, void* user_data);
```

## Error Handling

### Enhanced Error Reporting

libc4 provides comprehensive error reporting with position information and detailed context:

```c
const char* c4_error_string(c4_error_t err);
const c4_error_info_t* c4_get_last_error(void);
void c4_clear_error(void);
void c4_set_error_callback(c4_error_callback_t callback, void* user_data);
```

See [Error Handling Examples](#error-handling-examples) for usage.

## API Functions

### Initialization

#### `c4_init()`

Initialize the library. Must be called before using any other functions.

```c
void c4_init(void);
```

**Example:**
```c
int main(void) {
    c4_init();
    /* ... use library ... */
}
```

### ID Operations

#### `c4id_parse()`

Parse a C4 ID from a 90-character string.

```c
c4_id_t* c4id_parse(const char* src);
```

**Parameters:**
- `src` - Null-terminated string containing a C4 ID (must be exactly 90 characters)

**Returns:**
- Pointer to newly allocated `c4_id_t` on success
- `NULL` on error (check `c4_get_last_error()` for details)

**Example:**
```c
const char* id_str = "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdCbYyRZx4yYnBNkbPYFmX8gvgJE2k34N5ADC7H5CCa";
c4_id_t* id = c4id_parse(id_str);
if (id == NULL) {
    const c4_error_info_t* err = c4_get_last_error();
    fprintf(stderr, "Parse error: %s\n", err->detail);
    return -1;
}

/* Use id... */
c4id_free(id);
```

#### `c4id_string()`

Convert a C4 ID to its 90-character string representation.

```c
char* c4id_string(const c4_id_t* id);
```

**Parameters:**
- `id` - C4 ID to convert

**Returns:**
- Newly allocated string (caller must `free()`)
- `NULL` on error

**Example:**
```c
char* str = c4id_string(id);
if (str) {
    printf("ID: %s\n", str);
    free(str);
}
```

#### `c4id_cmp()`

Compare two C4 IDs.

```c
int c4id_cmp(const c4_id_t* a, const c4_id_t* b);
```

**Parameters:**
- `a`, `b` - IDs to compare

**Returns:**
- `-1` if `a < b`
- `0` if `a == b`
- `1` if `a > b`

**Example:**
```c
c4_id_t* id1 = c4id_parse(str1);
c4_id_t* id2 = c4id_parse(str2);

int result = c4id_cmp(id1, id2);
if (result == 0) {
    printf("IDs are equal\n");
} else if (result < 0) {
    printf("id1 < id2\n");
} else {
    printf("id1 > id2\n");
}
```

#### `c4id_free()`

Free a C4 ID.

```c
void c4id_free(c4_id_t* id);
```

**Parameters:**
- `id` - ID to free (safe to pass `NULL`)

### Encoder Operations

The encoder provides streaming hash computation for generating C4 IDs from data.

#### `c4id_encoder_new()`

Create a new encoder.

```c
c4id_encoder_t* c4id_encoder_new(void);
```

**Returns:**
- Pointer to newly allocated encoder
- `NULL` on allocation failure

**Example:**
```c
c4id_encoder_t* enc = c4id_encoder_new();
if (!enc) {
    fprintf(stderr, "Failed to create encoder\n");
    return -1;
}
```

#### `c4id_encoder_write()`

Write data to the encoder.

```c
void c4id_encoder_write(c4id_encoder_t* enc, const void* data, size_t size);
```

**Parameters:**
- `enc` - Encoder instance
- `data` - Data to hash
- `size` - Number of bytes to hash

**Example:**
```c
/* Hash multiple chunks */
c4id_encoder_write(enc, chunk1, size1);
c4id_encoder_write(enc, chunk2, size2);
c4id_encoder_write(enc, chunk3, size3);
```

#### `c4id_encoder_id()`

Get the final C4 ID for all data written to the encoder.

```c
c4_id_t* c4id_encoder_id(c4id_encoder_t* enc);
```

**Parameters:**
- `enc` - Encoder instance

**Returns:**
- Pointer to newly allocated `c4_id_t`
- `NULL` on error

**Note:** This finalizes the encoder. To reuse it, call `c4id_encoder_reset()`.

**Example:**
```c
c4_id_t* id = c4id_encoder_id(enc);
char* str = c4id_string(id);
printf("Final ID: %s\n", str);
free(str);
c4id_free(id);
```

#### `c4id_encoder_digest()`

Get the digest without finalizing the encoder.

```c
c4_digest_t* c4id_encoder_digest(c4id_encoder_t* enc);
```

**Parameters:**
- `enc` - Encoder instance

**Returns:**
- Pointer to newly allocated digest
- Can continue writing to encoder after this call

**Example:**
```c
/* Get intermediate digest */
c4_digest_t* digest = c4id_encoder_digest(enc);

/* Can continue writing */
c4id_encoder_write(enc, more_data, more_size);

c4id_digest_free(digest);
```

#### `c4id_encoder_reset()`

Reset the encoder to identify a new block of data.

```c
void c4id_encoder_reset(c4id_encoder_t* enc);
```

**Parameters:**
- `enc` - Encoder to reset

**Example:**
```c
/* Hash first file */
c4id_encoder_write(enc, file1_data, file1_size);
c4_id_t* id1 = c4id_encoder_id(enc);

/* Reuse encoder for second file */
c4id_encoder_reset(enc);
c4id_encoder_write(enc, file2_data, file2_size);
c4_id_t* id2 = c4id_encoder_id(enc);
```

#### `c4id_encoder_free()`

Free an encoder.

```c
void c4id_encoder_free(c4id_encoder_t* enc);
```

**Parameters:**
- `enc` - Encoder to free (safe to pass `NULL`)

### Digest Operations

Digests are 64-byte SHA-512 hashes that can be combined to create hierarchical IDs.

#### `c4id_digest_new()`

Create a digest from bytes.

```c
c4_digest_t* c4id_digest_new(const void* data, size_t length);
```

**Parameters:**
- `data` - Byte data
- `length` - Number of bytes (must be ≤ 64)

**Returns:**
- Newly allocated digest (zero-padded if `length < 64`)
- `NULL` if `length > 64`

**Example:**
```c
unsigned char hash[64] = { /* ... SHA-512 hash ... */ };
c4_digest_t* digest = c4id_digest_new(hash, 64);
```

#### `c4id_digest_sum()`

Combine two digests using C4's ordered concatenation and hashing.

```c
c4_digest_t* c4id_digest_sum(const c4_digest_t* left, const c4_digest_t* right);
```

**Parameters:**
- `left`, `right` - Digests to combine

**Returns:**
- Newly allocated combined digest
- `NULL` on error

**Example:**
```c
/* Create hierarchical ID from file chunks */
c4_digest_t* d1 = /* ... digest of chunk 1 ... */;
c4_digest_t* d2 = /* ... digest of chunk 2 ... */;
c4_digest_t* combined = c4id_digest_sum(d1, d2);

c4_id_t* id = c4id_digest_to_id(combined);
```

#### `c4id_digest_to_id()`

Convert a digest to a C4 ID without rehashing.

```c
c4_id_t* c4id_digest_to_id(const c4_digest_t* digest);
```

**Parameters:**
- `digest` - Digest to convert

**Returns:**
- Newly allocated C4 ID
- `NULL` on error

#### `c4id_digest_cmp()`

Compare two digests lexicographically.

```c
int c4id_digest_cmp(const c4_digest_t* a, const c4_digest_t* b);
```

**Parameters:**
- `a`, `b` - Digests to compare

**Returns:**
- `-1` if `a < b`
- `0` if `a == b`
- `1` if `a > b`

#### `c4id_digest_free()`

Free a digest.

```c
void c4id_digest_free(c4_digest_t* digest);
```

**Parameters:**
- `digest` - Digest to free (safe to pass `NULL`)

## Examples

### Example 1: Hash a File

```c
#include <c4.h>
#include <stdio.h>
#include <stdlib.h>

c4_id_t* hash_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    c4id_encoder_t* enc = c4id_encoder_new();
    if (!enc) {
        fclose(f);
        return NULL;
    }

    unsigned char buffer[4096];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        c4id_encoder_write(enc, buffer, bytes_read);
    }

    fclose(f);

    c4_id_t* id = c4id_encoder_id(enc);
    c4id_encoder_free(enc);

    return id;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    c4_init();

    c4_id_t* id = hash_file(argv[1]);
    if (!id) {
        fprintf(stderr, "Error hashing file\n");
        return 1;
    }

    char* str = c4id_string(id);
    printf("%s  %s\n", str, argv[1]);

    free(str);
    c4id_free(id);

    return 0;
}
```

### Example 2: Validate a C4 ID

```c
#include <c4.h>
#include <stdio.h>

int validate_id(const char* id_str) {
    c4_id_t* id = c4id_parse(id_str);

    if (!id) {
        const c4_error_info_t* err = c4_get_last_error();
        printf("Invalid ID: %s\n", err->detail);
        if (err->position >= 0) {
            printf("Error at position %d\n", err->position);
        }
        return 0;
    }

    /* Verify round-trip */
    char* str = c4id_string(id);
    int valid = (strcmp(str, id_str) == 0);

    free(str);
    c4id_free(id);

    return valid;
}

int main(void) {
    c4_init();

    const char* test_ids[] = {
        "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdCbYyRZx4yYnBNkbPYFmX8gvgJE2k34N5ADC7H5CCa",
        "c4invalid",
        NULL
    };

    for (int i = 0; test_ids[i]; i++) {
        printf("Testing: %s\n", test_ids[i]);
        if (validate_id(test_ids[i])) {
            printf("  ✓ Valid\n");
        } else {
            printf("  ✗ Invalid\n");
        }
    }

    return 0;
}
```

### Example 3: Create Hierarchical IDs

```c
#include <c4.h>
#include <stdio.h>

/* Create ID tree from multiple data blocks */
c4_id_t* create_tree_id(const char** blocks, size_t count) {
    if (count == 0) return NULL;

    /* Hash each block */
    c4_digest_t** digests = malloc(count * sizeof(c4_digest_t*));
    c4id_encoder_t* enc = c4id_encoder_new();

    for (size_t i = 0; i < count; i++) {
        c4id_encoder_reset(enc);
        c4id_encoder_write(enc, blocks[i], strlen(blocks[i]));
        digests[i] = c4id_encoder_digest(enc);
    }
    c4id_encoder_free(enc);

    /* Combine digests hierarchically */
    while (count > 1) {
        size_t next_count = (count + 1) / 2;

        for (size_t i = 0; i < next_count; i++) {
            size_t left_idx = i * 2;
            size_t right_idx = left_idx + 1;

            c4_digest_t* combined;
            if (right_idx < count) {
                combined = c4id_digest_sum(digests[left_idx], digests[right_idx]);
                c4id_digest_free(digests[right_idx]);
            } else {
                combined = digests[left_idx];
            }

            if (i != left_idx) {
                c4id_digest_free(digests[left_idx]);
            }

            digests[i] = combined;
        }

        count = next_count;
    }

    c4_id_t* id = c4id_digest_to_id(digests[0]);
    c4id_digest_free(digests[0]);
    free(digests);

    return id;
}

int main(void) {
    c4_init();

    const char* blocks[] = {
        "Block 1 data",
        "Block 2 data",
        "Block 3 data",
        "Block 4 data"
    };

    c4_id_t* id = create_tree_id(blocks, 4);
    char* str = c4id_string(id);

    printf("Tree ID: %s\n", str);

    free(str);
    c4id_free(id);

    return 0;
}
```

## Error Handling Examples

### Basic Error Checking

```c
c4_id_t* id = c4id_parse(user_input);
if (!id) {
    const c4_error_info_t* err = c4_get_last_error();
    fprintf(stderr, "Error in %s: %s\n", err->function, err->detail);
    return -1;
}
```

### Custom Error Handler

```c
void my_error_handler(const c4_error_info_t* error, void* user_data) {
    FILE* log = (FILE*)user_data;

    fprintf(log, "[ERROR] %s in %s()\n", error->message, error->function);

    if (error->position >= 0) {
        fprintf(log, "  Position: %d\n", error->position);
    }

    if (error->detail[0] != '\0') {
        fprintf(log, "  Details: %s\n", error->detail);
    }

    fflush(log);
}

int main(void) {
    c4_init();

    FILE* error_log = fopen("errors.log", "w");
    c4_set_error_callback(my_error_handler, error_log);

    /* All errors will be logged to file */
    /* ... use library ... */

    c4_set_error_callback(NULL, NULL);
    fclose(error_log);

    return 0;
}
```

## Legacy API

The following functions are deprecated but maintained for backward compatibility:

- `c4id_release()` → use `c4id_free()`
- `c4id_new_encoder()` → use `c4id_encoder_new()`
- `c4id_encoder_update()` → use `c4id_encoder_write()`
- `c4id_encoded_id()` → use `c4id_encoder_id()`
- `c4id_release_encoder()` → use `c4id_encoder_free()`
- `c4id_digest_id()` → use `c4id_digest_to_id()`

New code should use the modern API.

## Thread Safety

- Each thread has its own error state (thread-local storage)
- Opaque types (`c4_id_t`, `c4id_encoder_t`, `c4_digest_t`) are not thread-safe
- Do not share these objects between threads without external synchronization
- `c4_init()` should be called once before any threading

## License

See LICENSE file for details.

## See Also

- [C4 Specification (SMPTE ST 2114:2017)](https://www.smpte.org/)
- [Go Reference Implementation](https://github.com/Avalanche-io/c4)
- [Implementation Summary](IMPLEMENTATION_SUMMARY.md)
