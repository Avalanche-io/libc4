// SPDX-License-Identifier: Apache-2.0
// Cross-implementation conformance: a directory with no listed children and
// a null size (2026-11-11). The files under testdata/childless-dir-vector/
// are byte-identical copies of the Go reference repo's
// c4m/testdata/childless-dir-vector; see its README.md. What the size
// derives to depends on the directory's C4 ID alone:
//
//   empty/      the empty listing's ID  -> 0: known to hold nothing
//   collapsed/  any other ID           -> null: its listing lives elsewhere
//   unread/     no ID                  -> null: nobody listed it
//
// Through 2026-11-11 a collapsed directory derived to 0, and ComputeC4ID
// hashed that invented size.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#ifndef C4M_CHILDLESS_DIR_VECTOR_DIR
#error "C4M_CHILDLESS_DIR_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::string readAll(const std::string &name) {
    std::ifstream f(std::filesystem::path(C4M_CHILDLESS_DIR_VECTOR_DIR) / name, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string rootID() {
    std::string s = readAll("root-id.txt");
    while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

} // namespace

TEST_CASE("childless-dir vector: both forms resolve to the root ID", "[c4m][vector]") {
    auto form = GENERATE(as<std::string>{}, "stated.c4m", "normalized.c4m");
    INFO(form);
    CHECK(c4m::Manifest::Parse(readAll(form)).ComputeC4ID().String() == rootID());
}

TEST_CASE("childless-dir vector: sizes derive from the C4 ID alone", "[c4m][vector]") {
    auto form = GENERATE(as<std::string>{}, "stated.c4m", "normalized.c4m");
    INFO(form);
    auto m = c4m::Manifest::Parse(readAll(form));
    m.Canonicalize();
    std::map<std::string, int64_t> got;
    for (const auto &e : m.Entries())
        got[e.name] = e.size;
    const std::map<std::string, int64_t> want = {{"empty/", 0}, {"collapsed/", -1}, {"unread/", -1}};
    CHECK(got == want);
}

TEST_CASE("childless-dir vector: the canonicalized stated form writes the normalized file", "[c4m][vector]") {
    auto m = c4m::Manifest::Parse(readAll("stated.c4m"));
    m.Canonicalize();
    CHECK(m.Encode() == readAll("normalized.c4m"));
}

TEST_CASE("childless-dir vector: the normalized file's own bytes are the description", "[c4m][vector]") {
    CHECK(c4::ID::Identify(std::string_view(readAll("normalized.c4m"))).String() == rootID());
}
