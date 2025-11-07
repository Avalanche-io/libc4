/*
 * Fuzzing target for c4id_parse()
 * Tests robustness of ID parsing with arbitrary input
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

    /* Need at least some data and room for null terminator */
    if (size == 0 || size > 1000) {
        return 0;
    }

    /* Create null-terminated string from fuzzer input */
    char *input = (char *)malloc(size + 1);
    if (!input) {
        return 0;
    }

    memcpy(input, data, size);
    input[size] = '\0';

    /* Try to parse the input - should not crash on any input */
    c4_id_t *id = c4id_parse(input);

    if (id != NULL) {
        /* If parsing succeeded, try operations on the ID */
        char *str = c4id_string(id);
        if (str != NULL) {
            free(str);
        }

        /* Try to parse the string representation again */
        c4_id_t *id2 = c4id_parse(input);
        if (id2 != NULL) {
            /* Compare the IDs */
            c4id_cmp(id, id2);
            c4id_free(id2);
        }

        c4id_free(id);
    }

    free(input);
    return 0;
}
