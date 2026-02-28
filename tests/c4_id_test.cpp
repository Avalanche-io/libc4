// SPDX-License-Identifier: Apache-2.0
#include "c4/c4.hpp"
#include "c4/c4.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <sstream>
#include <string>

// =============================================================
// Test vectors from the Go reference implementation.
// These MUST match the Go output exactly.
// =============================================================

static const char *test_inputs[] = {
    "alfa", "bravo", "charlie", "delta", "echo",
    "foxtrot", "golf", "hotel", "india"
};

// Individual IDs for each input (Go test_vector_ids[0])
static const char *test_input_ids[] = {
    "c43zYcLni5LF9rR4Lg4B8h3Jp8SBwjcnyyeh4bc6gTPHndKuKdjUWx1kJPYhZxYt3zV6tQXpDs2shPsPYjgG81wZM1",
    "c42jd8KUQG9DKppN1qt5aWS3PAmdPmNutXyVTb8H123FcuU3shPxpUXsVdcouSALZ4PaDvMYzQSMYCWkb6rop9zhDa",
    "c44erLietE8C1iKmQ3y4ENqA9g82Exdkoxox3KEHops2ux5MTsuMjfbFRvUPsPdi9Pxc3C2MRvLxWT8eFw5XKbRQGw",
    "c42Sv2Wi2Qo8AKbJKnUP6YTSdz8pt9aDaf2Ltx44HF1UDdXANM8Ltk6qEzpncvmVbw6FZxgBumw9Eo2jtGyaQ5gDSC",
    "c41bviGCyTM2stoMYVTVKgBkfC6SitoLRFinp77BcmN9awdaeC9cxPy4zyFQBhmTvRzChawbECK1KBRnw3KnagA5be",
    "c427CsZdfUAHyQBS3hxDFrL9NqgKeRuKkuSkxuYTm26XG7AKAWCjViDuMhHaMmQBkvuHnsxojetbQU1DdxHjzyQw8r",
    "c41yLiwAPdsjiBAAw8AFwQGG3cAWnNbDio21NtHE8yD1Fh5irRE4FsccZvm1WdJ4FNHtR1kt5kev7wERsgYomaQbfs",
    "c44nNyaFuVbt5MCfo2PYWHpwMkBpYTbt14C6TuoLCYH5RLvAFLngER3nqHfXC2GuttcoDxGBi3pY1j3pUF2W3rZD8N",
    "c41nJ6CvPN7m7UkUA3oS2yjXYNSZ7WayxEQXWPae6wFkWwW8WChQWTu61bSeuCERu78BDK1LUEny1qHZnye3oU7DtY",
};

// Final tree ID for the 9 test inputs
static const char *expected_tree_id =
    "c435RzTWWsjWD1Fi7dxS3idJ7vFgPVR96oE95RfDDT5ue7hRSPENePDjPDJdnV46g7emDzWK8LzJUjGESMG5qzuXqq";

// =============================================================
// Basic identification tests
// =============================================================

TEST_CASE("C4 ID: identify empty string", "[c4][id]") {
    auto id = c4::ID::Identify("", 0);
    REQUIRE(id.String() ==
        "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdZrAkZzsWcbWtDg5oQstpDuni4Hirj75GEmTc1sFT");
}

TEST_CASE("C4 ID: identify 'foo' matches Go reference", "[c4][id]") {
    auto id = c4::ID::Identify("foo");
    REQUIRE(id.String() ==
        "c45xZeXwMSpqXjpDumcHMA6mhoAmGHkUo7r9WmN2UgSEQzj9KjgseaQdkEJ11fGb5S1WEENcV3q8RFWwEeVpC7Fjk2");
}

TEST_CASE("C4 ID: all test vector inputs match Go", "[c4][id]") {
    for (int i = 0; i < 9; i++) {
        auto id = c4::ID::Identify(test_inputs[i]);
        REQUIRE(id.String() == test_input_ids[i]);
    }
}

// =============================================================
// Parse and round-trip tests
// =============================================================

TEST_CASE("C4 ID: parse round-trip", "[c4][id]") {
    auto id = c4::ID::Identify("hello world");
    std::string str = id.String();
    REQUIRE(str.size() == 90);
    auto parsed = c4::ID::Parse(str);
    REQUIRE(id == parsed);
    REQUIRE(parsed.String() == str);
}

TEST_CASE("C4 ID: parse all test vectors round-trip", "[c4][id]") {
    for (int i = 0; i < 9; i++) {
        auto parsed = c4::ID::Parse(test_input_ids[i]);
        auto computed = c4::ID::Identify(test_inputs[i]);
        REQUIRE(parsed == computed);
        REQUIRE(parsed.String() == test_input_ids[i]);
    }
}

TEST_CASE("C4 ID: parse rejects invalid length", "[c4][id]") {
    REQUIRE_THROWS_AS(c4::ID::Parse("c4short"), std::invalid_argument);
}

TEST_CASE("C4 ID: parse rejects invalid prefix", "[c4][id]") {
    std::string bad(90, 'A');
    bad[0] = 'x';
    bad[1] = 'y';
    REQUIRE_THROWS_AS(c4::ID::Parse(bad), std::invalid_argument);
}

// =============================================================
// Edge case encodings from Go tests
// =============================================================

TEST_CASE("C4 ID: all-zero bytes encodes as all-1s", "[c4][id]") {
    uint8_t zeros[64] = {};
    auto id = c4::ID::FromDigest(zeros, 64);
    REQUIRE(id.String() ==
        "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111111");
}

TEST_CASE("C4 ID: all-0xFF bytes", "[c4][id]") {
    uint8_t all_ff[64];
    std::memset(all_ff, 0xFF, 64);
    auto id = c4::ID::FromDigest(all_ff, 64);
    REQUIRE(id.String() ==
        "c467rpwLCuS5DGA8KGZXKsVQ7dnPb9goRLoKfgGbLfQg9WoLUgNY77E2jT11fem3coV9nAkguBACzrU1iyZM4B8roQ");
}

TEST_CASE("C4 ID: specific byte patterns from Go", "[c4][id]") {
    struct TestCase {
        uint8_t bytes[64];
        const char *expected;
    };

    // From Go TestAppendOrder: tests base58 positional encoding
    TestCase cases[] = {
        {{}, "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111121"},
        {{}, "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111211"},
        {{}, "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111112111"},
        {{}, "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111121111"},
    };
    // Set specific trailing bytes
    cases[0].bytes[63] = 58;
    cases[1].bytes[62] = 0x0d; cases[1].bytes[63] = 0x24;
    cases[2].bytes[61] = 0x02; cases[2].bytes[62] = 0xfa; cases[2].bytes[63] = 0x28;
    cases[3].bytes[61] = 0xac; cases[3].bytes[62] = 0xad; cases[3].bytes[63] = 0x10;

    for (auto &tc : cases) {
        auto id = c4::ID::FromDigest(tc.bytes, 64);
        REQUIRE(id.String() == tc.expected);

        // Round-trip: parse and verify digest
        auto parsed = c4::ID::Parse(tc.expected);
        REQUIRE(std::memcmp(parsed.Digest().data(), tc.bytes, 64) == 0);
    }
}

// =============================================================
// FromDigest tests
// =============================================================

TEST_CASE("C4 ID: FromDigest", "[c4][id]") {
    auto id = c4::ID::Identify("test");
    auto reconstructed = c4::ID::FromDigest(id.Digest().data(), 64);
    REQUIRE(id == reconstructed);
    REQUIRE(id.String() == reconstructed.String());
}

TEST_CASE("C4 ID: FromDigest rejects wrong length", "[c4][id]") {
    uint8_t short_buf[32] = {};
    REQUIRE_THROWS_AS(c4::ID::FromDigest(short_buf, 32), std::invalid_argument);
}

// =============================================================
// Sum tests
// =============================================================

TEST_CASE("C4 ID: Sum is order-independent", "[c4][id]") {
    auto a = c4::ID::Identify("alfa");
    auto b = c4::ID::Identify("bravo");
    REQUIRE(a.Sum(b) == b.Sum(a));
}

TEST_CASE("C4 ID: Sum of identical IDs returns the same ID", "[c4][id]") {
    auto a = c4::ID::Identify("test");
    REQUIRE(a.Sum(a) == a);
}

TEST_CASE("C4 ID: Sum produces a different ID", "[c4][id]") {
    auto a = c4::ID::Identify("alfa");
    auto b = c4::ID::Identify("bravo");
    auto s = a.Sum(b);
    REQUIRE(s != a);
    REQUIRE(s != b);
}

// =============================================================
// Nil / Comparison tests
// =============================================================

TEST_CASE("C4 ID: nil ID", "[c4][id]") {
    c4::ID nil;
    REQUIRE(nil.IsNil());
    auto id = c4::ID::Identify("data");
    REQUIRE_FALSE(id.IsNil());
}

TEST_CASE("C4 ID: comparison", "[c4][id]") {
    auto a = c4::ID::Identify("alfa");
    auto b = c4::ID::Identify("bravo");
    REQUIRE(a != b);
    REQUIRE((a < b || b < a));
}

// =============================================================
// Stream identification
// =============================================================

TEST_CASE("C4 ID: stream identify matches direct", "[c4][id]") {
    std::istringstream stream("foo");
    auto id = c4::ID::Identify(stream);
    auto direct = c4::ID::Identify("foo");
    REQUIRE(id == direct);
}

TEST_CASE("C4 ID: digest access", "[c4][id]") {
    auto id = c4::ID::Identify("test");
    const auto &digest = id.Digest();
    REQUIRE(digest.size() == 64);
    bool all_zero = true;
    for (auto b : digest) {
        if (b != 0) { all_zero = false; break; }
    }
    REQUIRE_FALSE(all_zero);
}

// =============================================================
// C API tests
// =============================================================

TEST_CASE("C API: identify and string", "[c4][c-api]") {
    c4_id_t *id = c4_id_new();
    REQUIRE(id != nullptr);

    REQUIRE(c4_identify("foo", 3, id) == C4_OK);

    char buf[91];
    REQUIRE(c4_id_string(id, buf, sizeof(buf)) == C4_OK);
    REQUIRE(std::string(buf) ==
        "c45xZeXwMSpqXjpDumcHMA6mhoAmGHkUo7r9WmN2UgSEQzj9KjgseaQdkEJ11fGb5S1WEENcV3q8RFWwEeVpC7Fjk2");

    c4_id_free(id);
}

TEST_CASE("C API: parse round-trip", "[c4][c-api]") {
    c4_id_t *id = c4_id_new();
    const char *str =
        "c43zYcLni5LF9rR4Lg4B8h3Jp8SBwjcnyyeh4bc6gTPHndKuKdjUWx1kJPYhZxYt3zV6tQXpDs2shPsPYjgG81wZM1";
    REQUIRE(c4_id_parse(str, 90, id) == C4_OK);

    char buf[91];
    REQUIRE(c4_id_string(id, buf, sizeof(buf)) == C4_OK);
    REQUIRE(std::string(buf) == str);

    c4_id_free(id);
}

TEST_CASE("C API: from_digest round-trip", "[c4][c-api]") {
    c4_id_t *id = c4_id_new();
    REQUIRE(c4_identify("bravo", 5, id) == C4_OK);

    const uint8_t *digest = c4_id_digest(id);
    REQUIRE(digest != nullptr);

    c4_id_t *id2 = c4_id_new();
    REQUIRE(c4_id_from_digest(digest, 64, id2) == C4_OK);
    REQUIRE(c4_id_equal(id, id2));

    char buf1[91], buf2[91];
    c4_id_string(id, buf1, sizeof(buf1));
    c4_id_string(id2, buf2, sizeof(buf2));
    REQUIRE(std::string(buf1) == std::string(buf2));

    c4_id_free(id);
    c4_id_free(id2);
}

// =============================================================
// Tree ID tests
// =============================================================

TEST_CASE("Tree ID: matches Go reference", "[c4][tree]") {
    c4::IDs ids;
    for (const char *input : test_inputs) {
        ids.Append(c4::ID::Identify(input));
    }

    auto tree_id = ids.TreeID();
    REQUIRE(tree_id.String() == expected_tree_id);
}

TEST_CASE("Tree ID: order independent", "[c4][tree]") {
    auto make_set = [](std::initializer_list<const char *> inputs) {
        c4::IDs ids;
        for (const char *s : inputs) {
            ids.Append(c4::ID::Identify(s));
        }
        return ids.TreeID();
    };

    auto id1 = make_set({"alfa", "bravo", "charlie"});
    auto id2 = make_set({"charlie", "alfa", "bravo"});
    auto id3 = make_set({"bravo", "charlie", "alfa"});

    REQUIRE(id1 == id2);
    REQUIRE(id2 == id3);
}

TEST_CASE("Tree ID: single ID returns itself", "[c4][tree]") {
    c4::IDs ids;
    auto a = c4::ID::Identify("alfa");
    ids.Append(a);
    REQUIRE(ids.TreeID() == a);
}

TEST_CASE("Tree ID: empty set returns nil", "[c4][tree]") {
    c4::IDs ids;
    REQUIRE(ids.TreeID().IsNil());
}

TEST_CASE("Tree ID: duplicates are deduplicated", "[c4][tree]") {
    c4::IDs ids_with_dups;
    ids_with_dups.Append(c4::ID::Identify("alfa"));
    ids_with_dups.Append(c4::ID::Identify("alfa"));
    ids_with_dups.Append(c4::ID::Identify("bravo"));

    c4::IDs ids_without;
    ids_without.Append(c4::ID::Identify("alfa"));
    ids_without.Append(c4::ID::Identify("bravo"));

    REQUIRE(ids_with_dups.TreeID() == ids_without.TreeID());
}

TEST_CASE("Tree ID: two IDs equal to Sum", "[c4][tree]") {
    auto a = c4::ID::Identify("alfa");
    auto b = c4::ID::Identify("bravo");

    c4::IDs ids;
    ids.Append(a);
    ids.Append(b);

    // For 2 sorted IDs, tree ID should equal their Sum
    auto tree = ids.TreeID();
    auto sum = a.Sum(b);
    REQUIRE(tree == sum);
}
