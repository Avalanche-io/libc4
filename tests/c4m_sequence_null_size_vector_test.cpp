// SPDX-License-Identifier: Apache-2.0
// A folded sequence's size is the sum of its frames' sizes, and null if any
// frame's size is null: "-", or a 0 beside a C4 ID other than the empty
// content's (Entry::SizeIsNull). Ruled 2026-11-13 for c4 #155: "One cannot
// use size as an expectation if there are nulls present, so we must say
// nothing." Decided 2026-11-13 by the release lead for every port;
// reversible.
//
// The fixtures under testdata/sequence-null-size-vector/ are byte-identical
// copies of oss/c4/c4m/testdata/sequence-null-size-vector/ at c4 commit
// 7a6ba94 (#156).
//
// libc4 does not fold sequences: it has no sequence detector, and
// src/c4m/sequence.cpp is a stub. So only the decode half of the vector
// applies here: folded.c4m decodes to folded-root-id.txt, the plate range's
// size is a known 24 and the shot range's is null, and re-encoding gives
// folded.c4m byte for byte. The folding half (frames.c4m and
// frames-zero-spelled.c4m fold to folded.c4m) has nothing to test until
// libc4 folds.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_SEQUENCE_NULL_SIZE_VECTOR_DIR
#error "C4M_SEQUENCE_NULL_SIZE_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_SEQUENCE_NULL_SIZE_VECTOR_DIR) / name;
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

} // namespace

TEST_CASE("sequence null-size vector: folded.c4m decodes with the shot range's size null",
          "[c4m][vector][sequence][null-size]") {
    auto m = c4m::Manifest::Parse(std::string_view(readAll(fixture("folded.c4m"))));
    CHECK(m.ComputeC4ID().String() == readTrimmed(fixture("folded-root-id.txt")));

    const c4m::Entry *plate = m.GetEntry("plate.[1001-1004].exr");
    const c4m::Entry *shot = m.GetEntry("shot.[1001-1004].exr");
    REQUIRE(plate != nullptr);
    REQUIRE(shot != nullptr);
    CHECK(plate->is_sequence);
    CHECK(shot->is_sequence);
    CHECK_FALSE(plate->SizeIsNull());
    CHECK(plate->size == 24);
    CHECK(shot->SizeIsNull());
    CHECK(shot->size == c4m::NullSize);
}

TEST_CASE("sequence null-size vector: folded.c4m re-encodes canonically byte for byte",
          "[c4m][vector][sequence][null-size]") {
    const std::string folded = readAll(fixture("folded.c4m"));
    auto m = c4m::Manifest::Parse(std::string_view(folded));
    CHECK(m.Encode() == folded);
}
