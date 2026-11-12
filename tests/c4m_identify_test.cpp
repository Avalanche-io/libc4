// SPDX-License-Identifier: Apache-2.0
// Tests for c4m-aware identification: detection heuristic, canonical
// identification, and fallback to raw byte hashing.

#include "c4/c4.hpp"
#include "c4/c4.h"
#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>

// The detection, canonicalization and c4m-aware names are deprecated since
// 1.0.27; these tests pin what they still do.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// =============================================================
// Detection heuristic tests
// =============================================================

TEST_CASE("LooksLikeC4m: regular file entry", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("-rw-r--r-- 2025-01-15T10:30:00Z 1024 test.txt -"));
}

TEST_CASE("LooksLikeC4m: directory entry", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("drwxr-xr-x - - src/ -"));
}

TEST_CASE("LooksLikeC4m: symlink entry", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("lrwxrwxrwx - - link -> target -"));
}

TEST_CASE("LooksLikeC4m: named pipe entry", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("prw-r--r-- - - fifo -"));
}

TEST_CASE("LooksLikeC4m: socket entry", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("srwxrwxrwx - - sock -"));
}

TEST_CASE("LooksLikeC4m: block device entry", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("brw-rw---- - - sda -"));
}

TEST_CASE("LooksLikeC4m: char device entry", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("crw-rw-rw- - - null -"));
}

TEST_CASE("LooksLikeC4m: null mode dash", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("- - - test.txt -"));
}

TEST_CASE("LooksLikeC4m: leading spaces", "[c4m][detect]") {
    REQUIRE(c4m::LooksLikeC4m("  -rw-r--r-- - - nested.txt -"));
}

TEST_CASE("LooksLikeC4m: rejects empty", "[c4m][detect]") {
    REQUIRE_FALSE(c4m::LooksLikeC4m(""));
}

TEST_CASE("LooksLikeC4m: rejects whitespace only", "[c4m][detect]") {
    REQUIRE_FALSE(c4m::LooksLikeC4m("   "));
}

TEST_CASE("LooksLikeC4m: rejects JSON", "[c4m][detect]") {
    REQUIRE_FALSE(c4m::LooksLikeC4m("{\"key\": \"value\"}"));
}

TEST_CASE("LooksLikeC4m: rejects XML", "[c4m][detect]") {
    REQUIRE_FALSE(c4m::LooksLikeC4m("<root>content</root>"));
}

TEST_CASE("LooksLikeC4m: rejects shebang", "[c4m][detect]") {
    REQUIRE_FALSE(c4m::LooksLikeC4m("#!/bin/bash\necho hello"));
}

TEST_CASE("LooksLikeC4m: rejects number", "[c4m][detect]") {
    REQUIRE_FALSE(c4m::LooksLikeC4m("42 is the answer"));
}

// =============================================================
// C API detection heuristic
// =============================================================

TEST_CASE("C API: c4_looks_like_c4m positive", "[c4m][detect][c-api]") {
    const char *data = "-rw-r--r-- - - test.txt -";
    REQUIRE(c4_looks_like_c4m(data, strlen(data)) == 1);
}

TEST_CASE("C API: c4_looks_like_c4m negative", "[c4m][detect][c-api]") {
    const char *data = "{\"json\": true}";
    REQUIRE(c4_looks_like_c4m(data, strlen(data)) == 0);
}

TEST_CASE("C API: c4_looks_like_c4m null", "[c4m][detect][c-api]") {
    REQUIRE(c4_looks_like_c4m(nullptr, 0) == 0);
}

// =============================================================
// Canonicalization tests
// =============================================================

TEST_CASE("CanonicalizeC4m: valid c4m returns canonical", "[c4m][canonical]") {
    std::string canonical = c4m::CanonicalizeC4m("- - - hello.txt -\n");
    REQUIRE_FALSE(canonical.empty());
}

TEST_CASE("CanonicalizeC4m: invalid data returns empty", "[c4m][canonical]") {
    std::string result = c4m::CanonicalizeC4m("not a c4m file at all");
    REQUIRE(result.empty());
}

TEST_CASE("CanonicalizeC4m: empty input returns empty", "[c4m][canonical]") {
    REQUIRE(c4m::CanonicalizeC4m("").empty());
}

// =============================================================
// Two IDs for a c4m text (since 1.0.27): its byte ID is Identify of the
// text, like any other file; the ID of the description it holds is
// Manifest::Parse(text).ComputeC4ID(), the same for every form. Through
// 1.0.26 IdentifyC4mAware gave a third ID, the re-rendered nested text's.
// =============================================================

namespace {
c4::ID description(const std::string &text) { return c4m::Manifest::Parse(text).ComputeC4ID(); }
} // namespace

TEST_CASE("description ID: different indentation widths describe one tree", "[c4m][identify]") {
    std::string indent2 =
        "drwxr-xr-x - - src/ -\n"
        "  -rw-r--r-- - - main.cpp -\n";
    std::string indent4 =
        "drwxr-xr-x - - src/ -\n"
        "    -rw-r--r-- - - main.cpp -\n";
    REQUIRE(description(indent2) == description(indent4));
    REQUIRE(c4::ID::Identify(indent2) != c4::ID::Identify(indent4));
}

TEST_CASE("description ID: null mode forms describe one tree on this line", "[c4m][identify]") {
    // On 1.0.x ten dashes still decode as a null mode (the zero-versus-null
    // rule is held off this line).
    REQUIRE(description("- - - test.txt -\n") == description("---------- - - test.txt -\n"));
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

TEST_CASE("byte ID: non-canonical c4m text is identified by its bytes", "[c4m][identify]") {
    std::string non_canonical =
        "drwxr-xr-x - - src/ -\n"
        "    -rw-r--r-- - - main.cpp -\n";
    std::string canonical = c4m::Manifest::Parse(non_canonical).Encode();
    REQUIRE(canonical != non_canonical);
    REQUIRE(c4::ID::Identify(non_canonical) != c4::ID::Identify(canonical));
}

// =============================================================
// Deprecated since 1.0.27 (removed in 1.1): IdentifyC4mAware and
// c4_identify_c4m_aware are the bytes' ID.
// =============================================================

TEST_CASE("IdentifyC4mAware (deprecated) is Identify", "[c4m][identify]") {
    const std::string inputs[] = {
        std::string("-rw-r--r-- 2025-01-15T10:30:00Z 100 a.txt -\n"),
        std::string("drwxr-xr-x - - src/ -\n    -rw-r--r-- - - main.cpp -\n"),
        std::string("-rw-r--r-- - - z.txt -\n-rw-r--r-- - - a.txt -\n"),
        std::string("hello world"),
        std::string("\x00\x01\x02\x03", 4),
    };
    for (const auto &data : inputs) {
        REQUIRE(c4::ID::IdentifyC4mAware(data) == c4::ID::Identify(data));
        REQUIRE(c4::ID::IdentifyC4mAware(data.data(), data.size()) == c4::ID::Identify(data));
    }
}

TEST_CASE("C API: c4_identify_c4m_aware (deprecated) is c4_identify", "[c4m][identify][c-api]") {
    const char *unsorted = "-rw-r--r-- - - z.txt -\n-rw-r--r-- - - a.txt -\n";
    c4_id_t *aware = c4_id_new();
    c4_id_t *plain = c4_id_new();
    REQUIRE(c4_identify_c4m_aware(unsorted, strlen(unsorted), aware) == C4_OK);
    REQUIRE(c4_identify(unsorted, strlen(unsorted), plain) == C4_OK);
    REQUIRE(c4_id_equal(aware, plain) == 1);
    c4_id_free(aware);
    c4_id_free(plain);
}
