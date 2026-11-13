/* SPDX-License-Identifier: Apache-2.0
 *
 * Every function a public C header declares must link. This file is C, not
 * C++, so it also proves the headers compile as C. It takes the address of
 * each declared function (a declaration with no definition fails at link
 * time) and calls the basic round trip. tests/c_api_declared.cmake checks
 * that every function declared in include/c4/ *.h is named here, so a new
 * declaration cannot skip this test.
 *
 * Until 2026-11-13 include/c4/c4m.h declared a C API for c4m files that was
 * never implemented anywhere; it was removed rather than left promising
 * functions that do not exist. The c4m API is C++ (c4m.hpp).
 */
#include <c4/c4.h>

#include <stdio.h>
#include <string.h>

typedef void (*any_fn)(void);

int main(void) {
    const any_fn declared[] = {
        (any_fn)c4_id_new,
        (any_fn)c4_id_free,
        (any_fn)c4_identify,
        (any_fn)c4_identify_fd,
        (any_fn)c4_identify_file,
        (any_fn)c4_id_string,
        (any_fn)c4_id_parse,
        (any_fn)c4_id_compare,
        (any_fn)c4_id_equal,
        (any_fn)c4_id_digest,
        (any_fn)c4_id_from_digest,
        (any_fn)c4_tree_id,
    };
    for (size_t i = 0; i < sizeof(declared) / sizeof(declared[0]); i++) {
        if (declared[i] == NULL) {
            fprintf(stderr, "declared C function %zu has no address\n", i);
            return 1;
        }
    }

    /* The empty input's C4 ID, through the C API. */
    const char *want =
        "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdZrAkZzsWcbWtDg5oQstpDuni4Hirj75GEmTc1sFT";
    c4_id_t *id = c4_id_new();
    if (id == NULL || c4_identify("", 0, id) != C4_OK) {
        fprintf(stderr, "c4_identify failed\n");
        return 1;
    }
    char buf[C4_ID_LEN + 1];
    if (c4_id_string(id, buf, sizeof(buf)) != C4_OK || strcmp(buf, want) != 0) {
        fprintf(stderr, "empty input: got %s, want %s\n", buf, want);
        c4_id_free(id);
        return 1;
    }
    c4_id_t *back = c4_id_new();
    if (c4_id_parse(buf, C4_ID_LEN, back) != C4_OK || !c4_id_equal(id, back)) {
        fprintf(stderr, "c4_id_parse did not round-trip\n");
        c4_id_free(id);
        c4_id_free(back);
        return 1;
    }
    c4_id_free(id);
    c4_id_free(back);
    return 0;
}
