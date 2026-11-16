// SPDX-License-Identifier: Apache-2.0
// The range-data and sequence-size vectors, decode and normaliser half.
//
// The fixtures under testdata/range-data-vector/ and
// testdata/sequence-size-vector/ are byte-identical copies of
// oss/c4/c4m/testdata/ at c4 master b82ec07 (#159, #160, #161).
//
// Ruled 2026-11-13 by the maintainer: "A frame id list is a part of the c4m
// format, and is the same thing as a subdirectory being referenced by it's
// ID on a parents directory line", and "The size is the size of every
// frame and the frame id list." An output carries the list of every range
// it holds and no list for a range it does not; none is invented. A range's
// size is its frames' bytes plus count x 90; a directory holding a range
// counts it as one child, plus its listing as written.
//
// libc4 neither folds nor expands sequences, so the folding and expansion
// rows (#160) do not apply. Manifest::Parse resolving a chain and
// ApplyPatch keep the lists of the ranges their result holds. Merge,
// PatchDiff and DecodePatchChain/ResolvePatchChain carry no lists yet, so
// the vector's merge and diff rows are not met here.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>

#ifndef C4M_TESTDATA_DIR
#error "C4M_TESTDATA_DIR must be defined by the build"
#endif

namespace {

std::string readAll(const std::string &vector, const std::string &name) {
    std::ifstream f(std::filesystem::path(C4M_TESTDATA_DIR) / vector / name, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

c4m::Manifest parse(const std::string &vector, const std::string &name) {
    return c4m::Manifest::Parse(std::string_view(readAll(vector, name)));
}

const std::string RD = "range-data-vector";
const std::string SZ = "sequence-size-vector";

const c4m::Entry *find(const c4m::Manifest &m, const std::string &name) {
    for (const auto &e : m.Entries())
        if (e.name == name)
            return &e;
    return nullptr;
}

// nullSizes rebuilds m through the API with the named entries' sizes null,
// keeping its lists.
c4m::Manifest nullSizes(const c4m::Manifest &m, std::initializer_list<std::string> names) {
    c4m::Manifest out;
    for (c4m::Entry e : m.Entries()) {
        for (const auto &n : names)
            if (e.name == n)
                e.size = c4m::NullSize;
        out.AddEntry(std::move(e));
    }
    for (const auto &kv : m.RangeData())
        out.SetRangeData(kv.first, kv.second);
    return out;
}

} // namespace

TEST_CASE("range data vector: remove-chain.c4m resolves to no-plate.c4m, the removed range's list gone",
          "[c4m][vector][sequence][range-data]") {
    auto m = parse(RD, "remove-chain.c4m");
    CHECK(m.GetEntry("plate.[1001-1004].exr") == nullptr);
    CHECK(m.RangeData().size() == 1);
    CHECK(m.Encode() == readAll(RD, "no-plate.c4m"));
}

TEST_CASE("range data vector: add-chain.c4m resolves to folded.c4m with both lists",
          "[c4m][vector][sequence][range-data]") {
    auto m = parse(RD, "add-chain.c4m");
    CHECK(m.RangeData().size() == 2);
    CHECK(m.Encode() == readAll(RD, "folded.c4m"));
}

TEST_CASE("range data vector: ApplyPatch keeps the lists of the ranges its result holds",
          "[c4m][vector][sequence][range-data]") {
    // patch plain.c4m with add.c4m gives folded.c4m; patching folded.c4m
    // with remove.c4m's patch gives no-plate.c4m, without the plate list.
    auto added = c4m::ApplyPatch(parse(RD, "plain.c4m"), parse(RD, "add.c4m"));
    CHECK(added.Encode() == readAll(RD, "folded.c4m"));
    auto removed = c4m::ApplyPatch(parse(RD, "folded.c4m"), parse(RD, "remove.c4m"));
    CHECK(removed.RangeData().size() == 1);
    CHECK(removed.Encode() == readAll(RD, "no-plate.c4m"));
}

TEST_CASE("range data vector: folded-no-lists.c4m is valid and re-encodes as is, no list invented",
          "[c4m][vector][sequence][range-data]") {
    const std::string text = readAll(RD, "folded-no-lists.c4m");
    auto m = c4m::Manifest::Parse(std::string_view(text));
    CHECK(m.RangeData().empty());
    CHECK(m.Encode() == text);
}

TEST_CASE("range data vector: the lists never change a description ID",
          "[c4m][vector][sequence][range-data]") {
    auto folded = parse(RD, "folded.c4m");
    REQUIRE(folded.RangeData().size() == 2);
    CHECK(folded.ComputeC4ID() == parse(RD, "folded-no-lists.c4m").ComputeC4ID());
}

TEST_CASE("sequence size vector: the range is 303 and sub/ is 446",
          "[c4m][vector][sequence][size]") {
    auto m = parse(SZ, "folded.c4m");
    const c4m::Entry *sub = find(m, "sub/");
    const c4m::Entry *range = find(m, "f.[001-003].exr");
    REQUIRE(sub != nullptr);
    REQUIRE(range != nullptr);
    CHECK(range->is_sequence);
    CHECK(range->size == 303); // 33 bytes of frames + 3 x 90 of ID list
    CHECK(sub->size == 446);   // 303 + 143, sub/'s folded listing
}

TEST_CASE("sequence size vector: the normaliser derives sub/ as 446 and writes folded.c4m byte for byte",
          "[c4m][vector][sequence][size]") {
    const std::string text = readAll(SZ, "folded.c4m");
    auto m = nullSizes(c4m::Manifest::Parse(std::string_view(text)), {"sub/"});
    REQUIRE(find(m, "sub/")->SizeIsNull());
    m.Canonicalize();
    CHECK(find(m, "sub/")->size == 446);
    CHECK(m.Encode() == text);
}

TEST_CASE("sequence size vector: a null range size makes sub/ null under the normaliser",
          "[c4m][vector][sequence][size]") {
    auto m = nullSizes(parse(SZ, "folded.c4m"), {"f.[001-003].exr", "sub/"});
    m.Canonicalize();
    CHECK(find(m, "sub/")->SizeIsNull());
}

TEST_CASE("sequence size vector: expanded.c4m keeps the range at 303 and reads each frame's size as null",
          "[c4m][vector][sequence][size]") {
    auto m = parse(SZ, "expanded.c4m");
    CHECK(find(m, "f.[001-003].exr")->size == 303);
    for (const char *n : {"f.001.exr", "f.002.exr", "f.003.exr"}) {
        INFO(n);
        REQUIRE(find(m, n) != nullptr);
        CHECK(find(m, n)->SizeIsNull());
    }
}

// ---- Operations: the vector's MUST rows for merge, diff and patch ----
//
// Each output, encoded canonically, is the named file byte for byte. A
// two-way merge is a three-way merge over an empty base, as Go's
// Merge(nil, a, b). A patch is written by EncodePatch, as c4 diff writes
// it: the old state's C4 ID, then the entries, then the lists of the
// ranges it adds or changes.

TEST_CASE("range data vector: merge folded.c4m with plain.c4m, either order, is merged.c4m",
          "[c4m][vector][sequence][range-data][merge]") {
    const auto folded = parse(RD, "folded.c4m");
    const auto plain = parse(RD, "plain.c4m");
    for (int order = 0; order < 2; order++) {
        INFO("order " << order);
        auto r = order == 0 ? c4m::Merge(c4m::Manifest(), folded, plain)
                            : c4m::Merge(c4m::Manifest(), plain, folded);
        CHECK(r.conflicts.empty());
        CHECK(r.merged.RangeData().size() == 2);
        CHECK(r.merged.Encode() == readAll(RD, "merged.c4m"));
    }
}

TEST_CASE("range data vector: merge folded.c4m with itself is folded.c4m",
          "[c4m][vector][sequence][range-data][merge]") {
    const auto folded = parse(RD, "folded.c4m");
    auto r = c4m::Merge(c4m::Manifest(), folded, folded);
    CHECK(r.conflicts.empty());
    CHECK(r.merged.Encode() == readAll(RD, "folded.c4m"));
}

TEST_CASE("range data vector: a merge that removes the plate range drops its list",
          "[c4m][vector][sequence][range-data][merge]") {
    const auto folded = parse(RD, "folded.c4m");
    auto r = c4m::Merge(folded, parse(RD, "no-plate.c4m"), folded);
    CHECK(r.conflicts.empty());
    CHECK(r.merged.GetEntry("plate.[1001-1004].exr") == nullptr);
    CHECK(r.merged.Encode() == readAll(RD, "no-plate.c4m"));
}
