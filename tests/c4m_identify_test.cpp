// SPDX-License-Identifier: Apache-2.0
// Tests for c4m-aware identification: detection heuristic, canonical
// identification, and fallback to raw byte hashing.

#include "c4/c4.hpp"
#include "c4/c4.h"
#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

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
// c4m-aware identification: canonical equivalence
// =============================================================

TEST_CASE("IdentifyC4mAware: pretty and canonical produce same ID", "[c4m][identify]") {
    // Canonical form (no indentation, sorted)
    std::string canonical =
        "-rw-r--r-- 2025-01-15T10:30:00Z 100 a.txt -\n"
        "-rw-r--r-- 2025-01-15T10:30:00Z 200 b.txt -\n";

    // Pretty form (indentation, different order would sort the same)
    std::string pretty =
        "-rw-r--r-- 2025-01-15T10:30:00Z 100 a.txt -\n"
        "-rw-r--r-- 2025-01-15T10:30:00Z 200 b.txt -\n";

    auto id_canonical = c4::ID::IdentifyC4mAware(canonical);
    auto id_pretty = c4::ID::IdentifyC4mAware(pretty);
    REQUIRE(id_canonical == id_pretty);
}

TEST_CASE("IdentifyC4mAware: different indentation widths produce same ID", "[c4m][identify]") {
    // 2-space indentation (canonical)
    std::string indent2 =
        "drwxr-xr-x - - src/ -\n"
        "  -rw-r--r-- - - main.cpp -\n";

    // 4-space indentation (non-canonical)
    std::string indent4 =
        "drwxr-xr-x - - src/ -\n"
        "    -rw-r--r-- - - main.cpp -\n";

    auto id_2 = c4::ID::IdentifyC4mAware(indent2);
    auto id_4 = c4::ID::IdentifyC4mAware(indent4);
    REQUIRE(id_2 == id_4);
}

TEST_CASE("IdentifyC4mAware: null mode forms produce same ID", "[c4m][identify]") {
    // Null mode as single dash (canonical input)
    std::string dash = "- - - test.txt -\n";

    // Null mode as ten dashes (display format input)
    std::string ten_dash = "---------- - - test.txt -\n";

    auto id_dash = c4::ID::IdentifyC4mAware(dash);
    auto id_ten = c4::ID::IdentifyC4mAware(ten_dash);
    REQUIRE(id_dash == id_ten);
}

TEST_CASE("IdentifyC4mAware: unsorted entries produce same ID as sorted", "[c4m][identify]") {
    // Files in reverse order
    std::string unsorted =
        "-rw-r--r-- - - z.txt -\n"
        "-rw-r--r-- - - a.txt -\n";

    // Files in sorted order
    std::string sorted =
        "-rw-r--r-- - - a.txt -\n"
        "-rw-r--r-- - - z.txt -\n";

    auto id_unsorted = c4::ID::IdentifyC4mAware(unsorted);
    auto id_sorted = c4::ID::IdentifyC4mAware(sorted);
    REQUIRE(id_unsorted == id_sorted);
}

// =============================================================
// Non-c4m fallback: raw bytes
// =============================================================

TEST_CASE("IdentifyC4mAware: non-c4m falls through to raw hash", "[c4m][identify]") {
    std::string data = "hello world";
    auto id_aware = c4::ID::IdentifyC4mAware(data);
    auto id_raw = c4::ID::Identify(data);
    REQUIRE(id_aware == id_raw);
}

TEST_CASE("IdentifyC4mAware: JSON falls through to raw hash", "[c4m][identify]") {
    std::string data = "{\"key\": \"value\"}";
    auto id_aware = c4::ID::IdentifyC4mAware(data);
    auto id_raw = c4::ID::Identify(data);
    REQUIRE(id_aware == id_raw);
}

TEST_CASE("IdentifyC4mAware: binary data falls through", "[c4m][identify]") {
    std::string data = "\x00\x01\x02\x03\x04\x05";
    auto id_aware = c4::ID::IdentifyC4mAware(data);
    auto id_raw = c4::ID::Identify(data);
    REQUIRE(id_aware == id_raw);
}

TEST_CASE("IdentifyC4mAware: dash-prefixed non-c4m falls through", "[c4m][identify]") {
    // Starts with '-' (passes Phase 1) but won't parse as c4m (fails Phase 2)
    std::string data = "--- this is a diff file\n+++ not c4m content\n";
    auto id_aware = c4::ID::IdentifyC4mAware(data);
    auto id_raw = c4::ID::Identify(data);
    REQUIRE(id_aware == id_raw);
}

// =============================================================
// c4m-aware ID differs from raw byte ID for valid c4m
// =============================================================

TEST_CASE("IdentifyC4mAware: c4m ID differs from raw byte ID when non-canonical", "[c4m][identify]") {
    // 4-space indentation is non-canonical (canonical is 2-space)
    std::string non_canonical =
        "drwxr-xr-x - - src/ -\n"
        "    -rw-r--r-- - - main.cpp -\n";

    auto id_aware = c4::ID::IdentifyC4mAware(non_canonical);
    auto id_raw = c4::ID::Identify(non_canonical);

    // The c4m-aware ID should differ because canonicalization changes the bytes
    REQUIRE(id_aware != id_raw);
}

// =============================================================
// C API c4m-aware identification
// =============================================================

TEST_CASE("C API: c4_identify_c4m_aware non-c4m matches c4_identify", "[c4m][identify][c-api]") {
    const char *data = "hello world";
    size_t len = strlen(data);

    c4_id_t *id_aware = c4_id_new();
    c4_id_t *id_raw = c4_id_new();

    REQUIRE(c4_identify_c4m_aware(data, len, id_aware) == C4_OK);
    REQUIRE(c4_identify(data, len, id_raw) == C4_OK);
    REQUIRE(c4_id_equal(id_aware, id_raw));

    c4_id_free(id_aware);
    c4_id_free(id_raw);
}

TEST_CASE("C API: c4_identify_c4m_aware c4m content canonicalizes", "[c4m][identify][c-api]") {
    // Unsorted entries
    const char *unsorted =
        "-rw-r--r-- - - z.txt -\n"
        "-rw-r--r-- - - a.txt -\n";
    // Sorted entries
    const char *sorted_c4m =
        "-rw-r--r-- - - a.txt -\n"
        "-rw-r--r-- - - z.txt -\n";

    c4_id_t *id_unsorted = c4_id_new();
    c4_id_t *id_sorted = c4_id_new();

    REQUIRE(c4_identify_c4m_aware(unsorted, strlen(unsorted), id_unsorted) == C4_OK);
    REQUIRE(c4_identify_c4m_aware(sorted_c4m, strlen(sorted_c4m), id_sorted) == C4_OK);
    REQUIRE(c4_id_equal(id_unsorted, id_sorted));

    c4_id_free(id_unsorted);
    c4_id_free(id_sorted);
}
