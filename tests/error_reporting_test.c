/*
 * Test and demonstration of enhanced error reporting features
 */

#include <c4.h>
#include <stdio.h>
#include <stdlib.h>

/* Example error callback */
void error_handler(const c4_error_info_t* error, void* user_data) {
    const char* context = (const char*)user_data;

    fprintf(stderr, "\n=== Error Callback Triggered ===\n");
    if (context) {
        fprintf(stderr, "Context: %s\n", context);
    }
    fprintf(stderr, "Function: %s\n", error->function);
    fprintf(stderr, "Error Code: %d (%s)\n", error->code, error->message);
    if (error->position >= 0) {
        fprintf(stderr, "Position: %d\n", error->position);
    }
    if (error->detail[0] != '\0') {
        fprintf(stderr, "Details: %s\n", error->detail);
    }
    fprintf(stderr, "================================\n\n");
}

void test_null_input(void) {
    printf("Test 1: NULL input\n");
    c4_id_t* id = c4id_parse(NULL);

    if (id == NULL) {
        const c4_error_info_t* err = c4_get_last_error();
        printf("  Error: %s\n", err->message);
        printf("  Function: %s\n", err->function);
        printf("  Details: %s\n", err->detail);
    }
    printf("\n");
}

void test_invalid_length(void) {
    printf("Test 2: Invalid length\n");
    c4_id_t* id = c4id_parse("c4tooshort");

    if (id == NULL) {
        const c4_error_info_t* err = c4_get_last_error();
        printf("  Error: %s\n", err->message);
        printf("  Function: %s\n", err->function);
        printf("  Details: %s\n", err->detail);
    }
    printf("\n");
}

void test_invalid_prefix(void) {
    printf("Test 3: Invalid prefix\n");
    /* Create a 90-char string with wrong prefix */
    char bad_id[91];
    bad_id[0] = 'x';
    bad_id[1] = 'x';
    for (int i = 2; i < 90; i++) {
        bad_id[i] = '1';
    }
    bad_id[90] = '\0';

    c4_id_t* id = c4id_parse(bad_id);

    if (id == NULL) {
        const c4_error_info_t* err = c4_get_last_error();
        printf("  Error: %s\n", err->message);
        printf("  Function: %s\n", err->function);
        printf("  Position: %d\n", err->position);
        printf("  Details: %s\n", err->detail);
    }
    printf("\n");
}

void test_invalid_character(void) {
    printf("Test 4: Invalid character in ID\n");
    /* Create a 90-char string with invalid character */
    char bad_id[91] = "c4";
    for (int i = 2; i < 89; i++) {
        bad_id[i] = '1';
    }
    bad_id[50] = '0';  /* '0' is not valid in base58 */
    bad_id[89] = '1';
    bad_id[90] = '\0';

    c4_id_t* id = c4id_parse(bad_id);

    if (id == NULL) {
        const c4_error_info_t* err = c4_get_last_error();
        printf("  Error: %s\n", err->message);
        printf("  Function: %s\n", err->function);
        printf("  Position: %d\n", err->position);
        printf("  Details: %s\n", err->detail);
        printf("  Character at position %d: '%c'\n", err->position, bad_id[err->position]);
    }
    printf("\n");
}

void test_valid_id(void) {
    printf("Test 5: Valid ID (no error should be set)\n");
    const char* valid_id = "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdCbYyRZx4yYnBNkbPYFmX8gvgJE2k34N5ADC7H5CCa";

    c4_id_t* id = c4id_parse(valid_id);

    if (id != NULL) {
        const c4_error_info_t* err = c4_get_last_error();
        printf("  Parse successful!\n");
        printf("  Last error code: %d (%s)\n", err->code,
               err->code == C4_OK ? "C4_OK" : "unexpected");

        char* str = c4id_string(id);
        if (str) {
            printf("  Round-trip successful: %s\n",
                   strcmp(str, valid_id) == 0 ? "MATCH" : "MISMATCH");
            free(str);
        }

        c4id_free(id);
    }
    printf("\n");
}

void test_with_callback(void) {
    printf("Test 6: Error callback demonstration\n");

    /* Set up error callback */
    c4_set_error_callback(error_handler, "test_with_callback");

    /* Trigger an error */
    printf("  Attempting to parse invalid ID...\n");
    c4_id_t* id = c4id_parse("invalid");

    /* Clear callback */
    c4_set_error_callback(NULL, NULL);

    printf("  Error callback test complete\n\n");
}

int main(void) {
    printf("===========================================\n");
    printf("Enhanced Error Reporting Test Suite\n");
    printf("===========================================\n\n");

    c4_init();

    test_null_input();
    test_invalid_length();
    test_invalid_prefix();
    test_invalid_character();
    test_valid_id();
    test_with_callback();

    printf("===========================================\n");
    printf("All tests completed\n");
    printf("===========================================\n");

    return 0;
}
