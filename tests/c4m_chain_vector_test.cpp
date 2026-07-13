// SPDX-License-Identifier: Apache-2.0
// Cross-implementation chain-vector conformance test (grammar erratum,
// 2026-07-13). The fixtures under testdata/chain-vector/ are byte-identical
// copies of oss/c4/c4m/testdata/chain-vector/ -- the shared contract every
// c4m implementation must satisfy. See that directory's README.md.
//
//   vector.c4m         MUST resolve to exactly resolved-root-id.txt
//   bad-validator.c4m  MUST be rejected (closing validator mismatch)
//   bad-checkpoint.c4m MUST be rejected (interior checkpoint mismatch)

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_CHAIN_VECTOR_DIR
#error "C4M_CHAIN_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_CHAIN_VECTOR_DIR) / name;
}

// Read a file and strip trailing whitespace/newlines.
std::string readTrimmed(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

} // namespace

TEST_CASE("chain-vector: vector.c4m resolves to the pinned root ID", "[c4m][chain][vector]") {
    c4::ID expected = c4::ID::Parse(readTrimmed(fixture("resolved-root-id.txt")));

    c4m::Manifest m;
    REQUIRE_NOTHROW(m = c4m::Manifest::ParseFile(fixture("vector.c4m")));

    // The chain must be resolved, not flattened: base a.txt + patch adds b.txt.
    REQUIRE(m.EntryCount() == 2);
    REQUIRE(m.GetEntry("a.txt") != nullptr);
    REQUIRE(m.GetEntry("b.txt") != nullptr);

    // Resolved manifest identity equals the closing validator.
    REQUIRE(m.ComputeC4ID() == expected);
}

TEST_CASE("chain-vector: bad-validator.c4m is rejected", "[c4m][chain][vector]") {
    // The interior checkpoint is correct, but the closing validator at EOF
    // does not match the resolved manifest -> patch ID mismatch.
    REQUIRE_THROWS(c4m::Manifest::ParseFile(fixture("bad-validator.c4m")));
}

TEST_CASE("chain-vector: bad-checkpoint.c4m is rejected", "[c4m][chain][vector]") {
    // The interior checkpoint does not match the accumulated base state
    // -> patch ID mismatch (rejected before the closing validator).
    REQUIRE_THROWS(c4m::Manifest::ParseFile(fixture("bad-checkpoint.c4m")));
}
