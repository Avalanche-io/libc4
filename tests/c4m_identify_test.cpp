// SPDX-License-Identifier: Apache-2.0
// A c4m text has two IDs, and these tests keep them apart. Its byte ID is
// c4::ID::Identify of the text, like any other file; the ID of the
// description it holds is c4m::Manifest::Parse(text).ComputeC4ID(), which is
// the same however the description is formatted. Through 1.0.26 libc4 had a
// third, c4m-aware ID (the re-rendered nested text's), removed in 1.1.

#include "c4/c4.hpp"
#include "c4/c4.h"
#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>

namespace {
c4::ID description(const std::string &text) { return c4m::Manifest::Parse(text).ComputeC4ID(); }
} // namespace

// =============================================================
// The description ID is one for every form of a description
// =============================================================

TEST_CASE("description ID: different indentation widths describe one tree", "[c4m][identify]") {
    std::string indent2 =
        "drwxr-xr-x - - src/ -\n"
        "  -rw-r--r-- - - main.cpp -\n";
    std::string indent4 =
        "drwxr-xr-x - - src/ -\n"
        "    -rw-r--r-- - - main.cpp -\n";
    REQUIRE(description(indent2) == description(indent4));
    // Two texts, two byte IDs.
    REQUIRE(c4::ID::Identify(indent2) != c4::ID::Identify(indent4));
}

TEST_CASE("description ID: null mode and known zero mode differ", "[c4m][identify]") {
    // "-" is null; "----------" is the known zero (C4M-STANDARD 4.1).
    REQUIRE(description("- - - test.txt -\n") != description("---------- - - test.txt -\n"));
}

TEST_CASE("description ID: ten dashes on a directory line is a legacy null", "[c4m][identify]") {
    REQUIRE(description("- - - d/ -\n") == description("---------- - - d/ -\n"));
}

TEST_CASE("description ID: unsorted entries describe the sorted tree", "[c4m][identify]") {
    std::string unsorted =
        "-rw-r--r-- - - z.txt -\n"
        "-rw-r--r-- - - a.txt -\n";
    std::string sorted =
        "-rw-r--r-- - - a.txt -\n"
        "-rw-r--r-- - - z.txt -\n";
    REQUIRE(description(unsorted) == description(sorted));
    REQUIRE(c4::ID::Identify(unsorted) != c4::ID::Identify(sorted));
}

// =============================================================
// The byte ID is the bytes', c4m text or not
// =============================================================

TEST_CASE("byte ID: non-canonical c4m text is identified by its bytes", "[c4m][identify]") {
    std::string non_canonical =
        "drwxr-xr-x - - src/ -\n"
        "    -rw-r--r-- - - main.cpp -\n";
    // Identify hashes exactly the text given; nothing re-renders it.
    std::string canonical = c4m::Manifest::Parse(non_canonical).Encode();
    REQUIRE(canonical != non_canonical);
    REQUIRE(c4::ID::Identify(non_canonical) != c4::ID::Identify(canonical));
}

TEST_CASE("C API: c4_identify is the bytes' ID for c4m text", "[c4m][identify][c-api]") {
    const char *text = "-rw-r--r-- - - z.txt -\n-rw-r--r-- - - a.txt -\n";
    c4_id_t *id = c4_id_new();
    REQUIRE(c4_identify(text, std::strlen(text), id) == C4_OK);
    char buf[C4_ID_LEN + 1];
    REQUIRE(c4_id_string(id, buf, sizeof buf) == C4_OK);
    REQUIRE(std::string(buf) == c4::ID::Identify(std::string_view(text)).String());
    c4_id_free(id);
}
