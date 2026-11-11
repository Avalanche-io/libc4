// SPDX-License-Identifier: Apache-2.0
// The 1.0.x line reads the canonical form only, and refuses every pretty
// line, naming it. Through 1.0.25 a pretty line with a stated timestamp was
// refused ("cannot parse timestamp"), but one with a null timestamp parsed,
// and the column padding became part of the name and the link or flow
// target: the unicode-name vector's pretty.c4m read "café.txt" followed by
// spaces, and "目标.txt" followed by spaces as a link target, with no error.
#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_TESTDATA_DIR
#error "C4M_TESTDATA_DIR must be defined by the build"
#endif

namespace {

std::string readAll(const std::string &rel) {
    std::ifstream f(std::filesystem::path(C4M_TESTDATA_DIR) / rel, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

TEST_CASE("1.0.x refuses every pretty line, padded with a null or a stated timestamp", "[c4m][pretty]") {
    for (const char *file : {"unicode-name-vector/pretty.c4m", "link-target-vector/pretty.c4m"}) {
        INFO(file);
        try {
            (void)c4m::Manifest::Parse(readAll(file));
            FAIL_CHECK(file << " parsed instead of being refused");
        } catch (const std::runtime_error &e) {
            CHECK(std::string(e.what()).find("line 1") != std::string::npos);
        }
    }
}

TEST_CASE("1.0.x still reads the canonical forms of the same trees", "[c4m][pretty]") {
    for (const char *file : {"unicode-name-vector/canonical.c4m", "link-target-vector/canonical.c4m"}) {
        INFO(file);
        CHECK_NOTHROW(c4m::Manifest::Parse(readAll(file)));
    }
    c4m::Manifest m = c4m::Manifest::Parse(readAll("unicode-name-vector/canonical.c4m"));
    for (const auto &e : m.Entries()) {
        INFO(e.name);
        CHECK(e.name.find(' ') == std::string::npos);
        CHECK(e.target.find(' ') == std::string::npos);
        CHECK(e.flow_target.find(' ') == std::string::npos);
    }
}

TEST_CASE("an escaped space ending a name, then the field separator, is not padding", "[c4m][pretty]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.mode = 0644u;
    e.timestamp = 1767323045;
    e.size = 1;
    e.name = "trailing ";
    e.id = c4::ID::Identify(std::string("x"));
    m.AddEntry(e);
    std::string text = m.Encode();
    INFO(text);
    c4m::Manifest back = c4m::Manifest::Parse(text);
    REQUIRE(back.EntryCount() == 1);
    CHECK(back.Entries()[0].name == "trailing ");
}
