// SPDX-License-Identifier: Apache-2.0
// Cross-implementation deepen-vs-remove conformance test (erratum,
// 2026-07-31). The fixtures under testdata/deepen-vector/ are byte-identical
// copies of oss/c4/c4m/testdata/deepen-vector/ -- the shared contract every
// c4m implementation must satisfy. See that directory's README.md.
//
// Both files share a byte-identical base and restate the same directory
// line byte-identically; they differ only in whether a child follows it:
//
//   deepen.c4m  MUST resolve to a.txt, docs/, readme.md, guide.md
//               (a section that declares a child is not removing its parent)
//   remove.c4m  MUST resolve to a.txt alone
//               (restating a directory alone still deletes its subtree)
//
// Conformance is checked on the resolved entry set, not only on the ID: a
// non-verifying reader carrying the old rule returns the wrong manifest
// without ever reporting a mismatch.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef C4M_DEEPEN_VECTOR_DIR
#error "C4M_DEEPEN_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_DEEPEN_VECTOR_DIR) / name;
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

// A bare C4 ID line: exactly 90 characters starting "c4", no spaces.
bool isBareID(const std::string &line) {
    return line.size() == 90 && line.compare(0, 2, "c4") == 0 &&
           line.find(' ') == std::string::npos;
}

// Split a chain at its bare C4 ID lines into entry-only sections.
std::vector<std::string> sections(const std::string &text) {
    std::vector<std::string> out(1);
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (isBareID(line)) {
            out.emplace_back();
            continue;
        }
        if (line.empty())
            continue;
        out.back() += line + "\n";
    }
    while (!out.empty() && out.back().empty())
        out.pop_back();
    return out;
}

// Resolve the chain by applying each section to the accumulated state,
// through the public ApplyPatch, so the rule under test is exercised
// directly whether or not the parser resolves chains itself.
c4m::Manifest resolveByApply(const std::string &text) {
    auto secs = sections(text);
    REQUIRE(secs.size() == 2);
    c4m::Manifest m = c4m::Manifest::Parse(secs[0]);
    for (size_t i = 1; i < secs.size(); ++i)
        m = c4m::ApplyPatch(m, c4m::Manifest::Parse(secs[i]));
    return m;
}

void checkDeepen(const c4m::Manifest &m) {
    REQUIRE(m.EntryCount() == 4);
    REQUIRE(m.GetEntry("a.txt") != nullptr);
    REQUIRE(m.GetEntry("docs/") != nullptr);
    REQUIRE(m.GetEntry("docs/readme.md") != nullptr);
    REQUIRE(m.GetEntry("docs/guide.md") != nullptr);
    REQUIRE(m.ComputeC4ID() == c4::ID::Parse(readTrimmed(fixture("deepen-root-id.txt"))));
}

void checkRemove(const c4m::Manifest &m) {
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.GetEntry("a.txt") != nullptr);
    REQUIRE(m.GetEntry("docs/") == nullptr);
    REQUIRE(m.GetEntry("docs/readme.md") == nullptr);
    REQUIRE(m.ComputeC4ID() == c4::ID::Parse(readTrimmed(fixture("remove-root-id.txt"))));
}

} // namespace

TEST_CASE("deepen-vector: ApplyPatch deepens a directory restated with children", "[c4m][patch][vector]") {
    checkDeepen(resolveByApply(readAll(fixture("deepen.c4m"))));
}

TEST_CASE("deepen-vector: ApplyPatch removes a directory restated alone", "[c4m][patch][vector]") {
    checkRemove(resolveByApply(readAll(fixture("remove.c4m"))));
}

TEST_CASE("deepen-vector: the same restated line resolves to different states", "[c4m][patch][vector]") {
    auto deepen = resolveByApply(readAll(fixture("deepen.c4m")));
    auto remove = resolveByApply(readAll(fixture("remove.c4m")));
    REQUIRE(deepen.ComputeC4ID() != remove.ComputeC4ID());
}

#ifdef C4M_DEEPEN_VECTOR_CHAIN
// The resolving parser (checkpoints verified, closing validator verified)
// must accept both files and land on the pinned IDs. The 1.0.x parser does
// not resolve chains, so that line builds without this define.
TEST_CASE("deepen-vector: the resolving parser accepts deepen.c4m", "[c4m][chain][vector]") {
    c4m::Manifest m;
    REQUIRE_NOTHROW(m = c4m::Manifest::ParseFile(fixture("deepen.c4m")));
    checkDeepen(m);
}

TEST_CASE("deepen-vector: the resolving parser accepts remove.c4m", "[c4m][chain][vector]") {
    c4m::Manifest m;
    REQUIRE_NOTHROW(m = c4m::Manifest::ParseFile(fixture("remove.c4m")));
    checkRemove(m);
}
#endif
