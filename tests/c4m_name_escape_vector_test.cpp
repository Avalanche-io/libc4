// SPDX-License-Identifier: Apache-2.0
// Cross-implementation name-escape vector (c4 core, c4m/testdata/
// name-escape-vector; the files under testdata/name-escape-vector/ are
// byte-identical copies). In a written name a space is "\ ", a double quote
// is "\"", and a control byte is encoded by SafeName; a raw one is damage and
// is refused, naming the line. Through 1.0.26 libc4 read each into the name
// ("real.txt junk" became one name). Any consistent indent reads.
#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#ifndef C4M_NAME_ESCAPE_VECTOR_DIR
#error "C4M_NAME_ESCAPE_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_NAME_ESCAPE_VECTOR_DIR) / name;
}

} // namespace

TEST_CASE("name-escape vector: every damaged name is refused, naming line 1", "[c4m][vector][escape]") {
    for (const char *file : {"file-unescaped-space", "file-damaged-id-field", "file-raw-quote",
                             "file-raw-tab", "file-raw-control", "dir-unescaped-space"}) {
        INFO(file);
        try {
            (void)c4m::Manifest::ParseFile(fixture(std::string(file) + ".c4m"));
            FAIL_CHECK(file << " decoded instead of being refused");
        } catch (const std::runtime_error &e) {
            CHECK(std::string(e.what()).find("line 1") != std::string::npos);
        }
    }
}

TEST_CASE("name-escape vector: the ok files decode", "[c4m][vector][escape]") {
    CHECK(c4m::Manifest::ParseFile(fixture("ok-file.c4m")).EntryCount() == 1);
    CHECK(c4m::Manifest::ParseFile(fixture("ok-dir.c4m")).EntryCount() == 1);
    c4m::Manifest esc = c4m::Manifest::ParseFile(fixture("ok-escaped.c4m"));
    REQUIRE(esc.EntryCount() == 1);
    CHECK(esc.Entries()[0].name == "my file \"q\"\t.txt");
    c4m::Manifest tree = c4m::Manifest::ParseFile(fixture("ok-indent-3.c4m"));
    REQUIRE(tree.EntryCount() == 2);
    CHECK(tree.Entries()[0].name == "d/");
    CHECK(tree.Entries()[1].name == "x.txt");
    CHECK(tree.Entries()[1].depth == 1);
}

TEST_CASE("name-escape vector: escaped names round-trip", "[c4m][vector][escape]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.mode = 0644u;
    e.timestamp = 1767323045;
    e.size = 1;
    e.name = "my file \"q\"\t.txt";
    e.id = c4::ID::Identify(std::string("x"));
    m.AddEntry(e);
    std::string text = m.Encode();
    INFO(text);
    CHECK(text.find("my file") == std::string::npos);
    c4m::Manifest back = c4m::Manifest::Parse(text);
    REQUIRE(back.EntryCount() == 1);
    CHECK(back.Entries()[0].name == e.name);
}
