// SPDX-License-Identifier: Apache-2.0
// SafeName's printable test is pinned to Unicode 17.0.0 (c4 #149; decided
// 2026-11-13 by the release lead; reversible), and the shared SafeName
// vector: one name, one written form, in every implementation.
//
// The fixtures under testdata/safename-vector/ are byte-identical copies of
// oss/c4/c4m/testdata/safename-vector/. printable-17.0.0.txt is the table
// every implementation embeds: Go's unicode.IsPrint at Unicode 17.0.0, one
// inclusive hex range per line. libc4's copy, src/c4m/printable_table.inc,
// is generated from it by tools/gen_printable.cmake (the
// printable_table_generated test checks the two agree).

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef C4M_SAFENAME_VECTOR_DIR
#error "C4M_SAFENAME_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_SAFENAME_VECTOR_DIR) / name;
}

std::string readAll(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string readTrimmed(const std::filesystem::path &p) {
    std::string s = readAll(p);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::vector<std::string> lines(const std::string &text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string l; std::getline(in, l);)
        out.push_back(l);
    return out;
}

using Ranges = std::vector<std::pair<uint32_t, uint32_t>>;

Ranges readTable() {
    Ranges r;
    for (const auto &l : lines(readAll(fixture("printable-17.0.0.txt")))) {
        if (l.empty() || l[0] == '#')
            continue;
        auto dots = l.find("..");
        REQUIRE(dots != std::string::npos);
        r.emplace_back(static_cast<uint32_t>(std::stoul(l.substr(0, dots), nullptr, 16)),
                       static_cast<uint32_t>(std::stoul(l.substr(dots + 2), nullptr, 16)));
    }
    return r;
}

std::string utf8(uint32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

std::string unhex(const std::string &h) {
    std::string out;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
    return out;
}

struct Case {
    std::string label, raw, field;
};

std::vector<Case> readCases() {
    std::vector<Case> out;
    for (const auto &l : lines(readAll(fixture("cases.txt")))) {
        auto a = l.find(' ');
        auto b = l.find(' ', a + 1);
        REQUIRE(a != std::string::npos);
        REQUIRE(b != std::string::npos);
        out.push_back({l.substr(0, a), unhex(l.substr(a + 1, b - a - 1)), l.substr(b + 1)});
    }
    return out;
}

} // namespace

TEST_CASE("safename vector: pinned to Unicode 17.0.0", "[c4m][vector][safename]") {
    REQUIRE(readTrimmed(fixture("unicode-version.txt")) == "17.0.0");
    auto table = readTable();
    REQUIRE(table.size() == 741);
    for (size_t i = 0; i < table.size(); i++) {
        REQUIRE(table[i].first <= table[i].second);
        if (i > 0)
            REQUIRE(table[i - 1].second < table[i].first);
    }
}

// SafeName's Tier 1 is exactly the table: every code point the table calls
// printable is written as itself, and every other one is encoded. The two
// exceptions the rule names, U+00A4 (the Tier 3 delimiter) and U+005C (the
// Tier 2 sigil), are printable but always escaped. Surrogates are not in
// the table and have no valid UTF-8 form.
TEST_CASE("safename: Tier 1 is the pinned table, for every code point", "[c4m][safename]") {
    auto table = readTable();
    size_t ri = 0;
    size_t mismatches = 0;
    for (uint32_t cp = 0; cp <= 0x10FFFF; cp++) {
        while (ri < table.size() && table[ri].second < cp)
            ri++;
        bool printable = ri < table.size() && table[ri].first <= cp;
        if (cp == 0xA4 || cp == 0x5C)
            printable = false;
        const std::string s = utf8(cp);
        const bool asItself = c4m::SafeName(s) == s;
        if (asItself != printable) {
            if (mismatches++ < 10)
                FAIL_CHECK("U+" << std::hex << cp << ": table says " << (printable ? "printable" : "not printable")
                                << ", SafeName " << (asItself ? "writes it as itself" : "encodes it"));
        }
        // Whatever SafeName does, the name reads back.
        if (cp % 97 == 0 || !asItself)
            REQUIRE(c4m::UnsafeName(c4m::SafeName(s)) == s);
    }
    REQUIRE(mismatches == 0);
}

// U+0870 was assigned in Unicode 14.0 and is printable at 17.0.0, so it is
// written as itself. U+0378 is unassigned at 17.0.0, so it is encoded and
// reads back. Neither depends on the platform's Unicode.
TEST_CASE("safename: U+0870 is written as itself and U+0378 is encoded", "[c4m][safename]") {
    const std::string u0870 = "a" + utf8(0x0870) + "b";
    const std::string u0378 = "a" + utf8(0x0378) + "b";
    CHECK(c4m::SafeName(u0870) == u0870);
    CHECK(c4m::SafeName(u0378) != u0378);
    CHECK(c4m::SafeName(u0378) == "a\xC2\xA4\xE2\xA3\x8D\xE2\xA2\xB8\xC2\xA4" "b"); // a¤⣍⢸¤b
    CHECK(c4m::UnsafeName(c4m::SafeName(u0378)) == u0378);
}

// A real U+FFFD is printable (a symbol) and written as itself, as in Go.
// Through 2026-11-13 libc4 took it for invalid UTF-8 and wrote only its
// first byte in Tier 3, so the name lost two bytes and did not read back.
TEST_CASE("safename: a real U+FFFD is written as itself", "[c4m][safename]") {
    const std::string raw = "a" + utf8(0xFFFD) + "b";
    CHECK(c4m::SafeName(raw) == raw);
    CHECK(c4m::UnsafeName(c4m::SafeName(raw)) == raw);
    // An invalid byte is still encoded byte by byte.
    CHECK(c4m::SafeName("a\xFF" "b") == "a\xC2\xA4\xE2\xA3\xBF\xC2\xA4" "b"); // a¤⣿¤b
}

TEST_CASE("safename vector: the writer gives each case's field", "[c4m][vector][safename]") {
    auto cases = readCases();
    REQUIRE(cases.size() == 31);
    for (const auto &c : cases) {
        INFO(c.label);
        CHECK(c4m::EscapeField(c.raw, false) == c.field);
    }
}

TEST_CASE("safename vector: names.c4m reads back to the raw names and root-id.txt",
          "[c4m][vector][safename]") {
    auto cases = readCases();
    auto m = c4m::Manifest::Parse(std::string_view(readAll(fixture("names.c4m"))));
    REQUIRE(m.EntryCount() == cases.size());
    for (const auto &c : cases) {
        INFO(c.label);
        CHECK(m.GetEntry(c.raw) != nullptr);
    }
    CHECK(m.ComputeC4ID().String() == readTrimmed(fixture("root-id.txt")));
    // And the library writes names.c4m back byte for byte.
    CHECK(m.Encode() == readAll(fixture("names.c4m")));
}
