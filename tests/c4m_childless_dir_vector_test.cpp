// SPDX-License-Identifier: Apache-2.0
// Cross-implementation conformance: a directory with no listed children and
// a null size (2026-11-11). The files under testdata/childless-dir-vector/
// are byte-identical copies of the Go reference repo's
// c4m/testdata/childless-dir-vector; see its README.md. What the size
// derives to depends on the directory's C4 ID alone:
//
//   empty/      the empty listing's ID  -> 0: known to hold nothing
//   collapsed/  any other ID           -> null: its listing lives elsewhere
//   unread/     no ID                  -> 0 on 1.0.x; null on main
//
// Through 1.0.26 a collapsed directory derived to 0, and ComputeC4ID hashed
// that invented size. unread/ is a known 1.0.x difference, as on the Go
// reference's 1.0.27 line, so the committed normalized.c4m and root-id.txt
// are main's, and this line's own normalised form and ID are checked
// against themselves and against the 1.0.x ID pinned in every
// implementation.

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

const std::string kLineRootID =
    "c43hjzfaERSHQSgDYwRPabKdXuC1gbHSW1BnXPVhvCm8LNgYj5LEH6a3VoV2gckqiLNMBQEh4dTVKszriwrmZryUkh";

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

std::string lineNormalized() {
    auto m = c4m::Manifest::Parse(readAll("stated.c4m"));
    m.Canonicalize();
    return m.Encode();
}

} // namespace

TEST_CASE("childless-dir vector: sizes derive from the C4 ID alone", "[c4m][vector]") {
    auto form = GENERATE(as<std::string>{}, "stated.c4m", "normalized.c4m");
    INFO(form);
    auto m = c4m::Manifest::Parse(readAll(form));
    m.Canonicalize();
    std::map<std::string, int64_t> got;
    for (const auto &e : m.Entries())
        got[e.name] = e.size;
    const std::map<std::string, int64_t> want = {{"empty/", 0}, {"collapsed/", -1}, {"unread/", 0}};
    CHECK(got == want);
}

TEST_CASE("childless-dir vector: main's normalized file differs from this line's only in unread/'s size",
          "[c4m][vector]") {
    auto main = c4m::Manifest::Parse(readAll("normalized.c4m"));
    c4m::Manifest edited;
    for (auto e : main.Entries()) {
        if (e.name == "unread/")
            e.size = 0;
        edited.AddEntry(e);
    }
    CHECK(edited.Encode() == lineNormalized());
}

TEST_CASE("childless-dir vector: this line's ID is the pinned 1.0.x ID, agrees with itself, and differs from main's",
          "[c4m][vector]") {
    const std::string line = c4m::Manifest::Parse(readAll("stated.c4m")).ComputeC4ID().String();
    CHECK(line == kLineRootID);
    const std::string normalized = lineNormalized();
    CHECK(c4m::Manifest::Parse(normalized).ComputeC4ID().String() == line);
    CHECK(c4::ID::Identify(std::string_view(normalized)).String() == line);
    // If this ever matches, the unread/ difference is gone: update the test.
    CHECK(line != rootID());
}

TEST_CASE("childless-dir vector: the normalized file's own bytes are main's description", "[c4m][vector]") {
    CHECK(c4::ID::Identify(std::string_view(readAll("normalized.c4m"))).String() == rootID());
}
