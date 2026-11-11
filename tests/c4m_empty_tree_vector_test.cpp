// SPDX-License-Identifier: Apache-2.0
// Cross-implementation empty-tree conformance test (2026-11-11). The files
// under testdata/empty-tree-vector/ are byte-identical copies of the Go
// reference repo's c4m/testdata/empty-tree-vector; see its README.md.
//
// An empty listing is known to be empty, not unknown: its C4 ID is the C4
// ID of zero bytes, and a directory whose listing is empty carries it. A
// patch from the empty tree names that ID as its base.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_EMPTY_TREE_VECTOR_DIR
#error "C4M_EMPTY_TREE_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::string readAll(const std::string &name) {
    std::ifstream f(std::filesystem::path(C4M_EMPTY_TREE_VECTOR_DIR) / name, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string readTrimmed(const std::string &name) {
    std::string s = readAll(name);
    while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

c4m::Entry emptyDir() {
    auto m = c4m::Manifest::Parse(readAll("empty-dir.c4m"));
    REQUIRE(m.Entries().size() == 1);
    return m.Entries()[0];
}

} // namespace

TEST_CASE("empty-tree vector: the empty manifest's C4 ID is the ID of zero bytes", "[c4m][vector]") {
    auto want = readTrimmed("empty-id.txt");
    CHECK(c4::ID::Identify(std::string_view("")).String() == want);
    CHECK(c4m::Manifest().ComputeC4ID().String() == want);
    CHECK(c4m::Manifest::Parse(readAll("empty.c4m")).ComputeC4ID().String() == want);
}

TEST_CASE("empty-tree vector: the empty manifest encodes as no bytes", "[c4m][vector]") {
    CHECK(c4m::Manifest().Encode() == readAll("empty.c4m"));
}

TEST_CASE("empty-tree vector: an empty directory carries the empty listing's ID", "[c4m][vector]") {
    auto form = GENERATE(as<std::string>{}, "empty-dir.c4m");
    INFO(form);
    auto m = c4m::Manifest::Parse(readAll(form));
    REQUIRE(m.Entries().size() == 1);
    CHECK(m.Entries()[0].name == "empty/");
    CHECK(m.Entries()[0].size == 0);
    CHECK(m.Entries()[0].id.String() == readTrimmed("empty-id.txt"));
    CHECK(m.ComputeC4ID().String() == readTrimmed("empty-dir-root-id.txt"));
}

TEST_CASE("empty-tree vector: a patch from the empty tree names its ID as base", "[c4m][vector]") {
    auto sections = c4m::DecodePatchChain(std::string_view(readAll("empty-dir-patch.c4m")));
    REQUIRE(sections.size() == 1);
    CHECK(sections[0].baseID.String() == readTrimmed("empty-id.txt"));
    REQUIRE(sections[0].entries.size() == 1);

    // Written by this library: the same base line first.
    c4m::Manifest after;
    after.AddEntry(emptyDir());
    auto pr = c4m::PatchDiff(c4m::Manifest(), after);
    CHECK(pr.oldID.String() == readTrimmed("empty-id.txt"));
    auto written = c4m::EncodePatch(pr);
    CHECK(written.substr(0, written.find('\n')) == readTrimmed("empty-id.txt"));
}
