/*
 * Fuzzing target for c4id_encoder_*()
 * Tests robustness of encoder with arbitrary data
 */

#include <c4.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Initialize library on first run */
    static int initialized = 0;
    if (!initialized) {
        c4_init();
        initialized = 1;
    }

    /* Limit size to prevent excessive memory usage */
    if (size == 0 || size > 100000) {
        return 0;
    }

    /* Test basic encoding */
    c4id_encoder_t *enc = c4id_encoder_new();
    if (!enc) {
        return 0;
    }

    /* Write data in various chunk sizes */
    if (size > 0) {
        size_t chunk_size = 1;
        size_t offset = 0;

        /* Use first byte to determine chunking strategy */
        if (data[0] % 3 == 0) {
            chunk_size = 1;  /* Byte by byte */
        } else if (data[0] % 3 == 1) {
            chunk_size = 16; /* Medium chunks */
        } else {
            chunk_size = size; /* All at once */
        }

        while (offset < size) {
            size_t to_write = chunk_size;
            if (offset + to_write > size) {
                to_write = size - offset;
            }
            c4id_encoder_write(enc, data + offset, to_write);
            offset += to_write;
        }
    }

    /* Get the digest without finalizing */
    c4_digest_t *digest = c4id_encoder_digest(enc);
    if (digest) {
        c4id_digest_free(digest);
    }

    /* Get the final ID */
    c4_id_t *id = c4id_encoder_id(enc);
    if (id) {
        char *str = c4id_string(id);
        if (str) {
            free(str);
        }
        c4id_free(id);
    }

    /* Test reset and reuse */
    c4id_encoder_reset(enc);
    if (size > 0) {
        c4id_encoder_write(enc, data, size);
        c4_id_t *id2 = c4id_encoder_id(enc);
        if (id2) {
            c4id_free(id2);
        }
    }

    c4id_encoder_free(enc);
    return 0;
}
