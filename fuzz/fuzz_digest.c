/*
 * Fuzzing target for c4_digest_*()
 * Tests robustness of digest operations with arbitrary data
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

    /* Need at least 1 byte for operations */
    if (size == 0) {
        return 0;
    }

    /* Limit size to prevent excessive memory usage */
    if (size > 200) {
        size = 200;
    }

    /* Test digest creation with various lengths */
    c4_digest_t *d1 = c4id_digest_new(data, size > 64 ? 64 : size);
    if (!d1) {
        return 0;
    }

    /* Test with second digest if we have enough data */
    if (size >= 2) {
        size_t len2 = size / 2;
        if (len2 > 64) len2 = 64;

        c4_digest_t *d2 = c4id_digest_new(data + (size / 2), len2);
        if (d2) {
            /* Test digest sum */
            c4_digest_t *sum1 = c4id_digest_sum(d1, d2);
            c4_digest_t *sum2 = c4id_digest_sum(d2, d1);

            if (sum1 && sum2) {
                /* Compare digests */
                c4id_digest_cmp(sum1, sum2);

                /* Convert to IDs */
                c4_id_t *id1 = c4id_digest_to_id(sum1);
                c4_id_t *id2 = c4id_digest_to_id(sum2);

                if (id1 && id2) {
                    /* Compare IDs */
                    c4id_cmp(id1, id2);

                    /* Get string representations */
                    char *str1 = c4id_string(id1);
                    char *str2 = c4id_string(id2);

                    if (str1) free(str1);
                    if (str2) free(str2);

                    c4id_free(id1);
                    c4id_free(id2);
                }

                c4id_digest_free(sum1);
                c4id_digest_free(sum2);
            }

            c4id_digest_free(d2);
        }
    }

    /* Test digest to ID conversion */
    c4_id_t *id = c4id_digest_to_id(d1);
    if (id) {
        c4id_free(id);
    }

    c4id_digest_free(d1);
    return 0;
}
