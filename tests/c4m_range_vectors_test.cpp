// SPDX-License-Identifier: Apache-2.0
// The range-data and sequence-size vectors: decode, operations and normaliser.
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
// Every operation that builds a c4m from c4m inputs carries the lists of
// the ranges its output holds: Merge, PatchDiff (written by EncodePatch),
// ApplyPatch, DecodePatchChain with ResolvePatchChain, FilterByPath and
// FilterByPrefix. This file runs every merge, diff and patch row of the
// vector's MUST table. Chains resolve through Manifest::Parse, as core
// release/1.0.27's decoder does, and through DecodePatchChain with
// ResolvePatchChain; both are run. libc4
// neither folds nor expands sequences: no such operation, so the two
// expansion rows (#160) have nothing to run here.

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

TEST_CASE("range data vector: Parse resolves remove-chain.c4m to no-plate.c4m, the removed range's list gone",
          "[c4m][vector][sequence][range-data][parse]") {
    auto m = parse(RD, "remove-chain.c4m");
    CHECK(m.GetEntry("plate.[1001-1004].exr") == nullptr);
    CHECK(m.RangeData().size() == 1);
    CHECK(m.Encode() == readAll(RD, "no-plate.c4m"));
}

TEST_CASE("range data vector: Parse resolves add-chain.c4m to folded.c4m with both lists",
          "[c4m][vector][sequence][range-data][parse]") {
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
    REQUIRE(find(m, "sub/")->size == c4m::NullSize);
    m.Canonicalize();
    CHECK(find(m, "sub/")->size == 446);
    CHECK(m.Encode() == text);
}

TEST_CASE("sequence size vector: a null range size makes sub/ null under the normaliser",
          "[c4m][vector][sequence][size]") {
    auto m = nullSizes(parse(SZ, "folded.c4m"), {"f.[001-003].exr", "sub/"});
    m.Canonicalize();
    CHECK(find(m, "sub/")->size == c4m::NullSize);
}

TEST_CASE("sequence size vector: expanded.c4m keeps the range at 303 and reads each frame's size as null",
          "[c4m][vector][sequence][size]") {
    auto m = parse(SZ, "expanded.c4m");
    CHECK(find(m, "f.[001-003].exr")->size == 303);
    for (const char *n : {"f.001.exr", "f.002.exr", "f.003.exr"}) {
        INFO(n);
        REQUIRE(find(m, n) != nullptr);
        CHECK(find(m, n)->size == c4m::NullSize);
    }
}

// ---- Operations: the vector's MUST rows for merge, diff and patch ----
//
// Each output, encoded canonically, is the named file byte for byte. A
// two-way merge is a three-way merge over an empty base, as Go's
// Merge(nil, a, b). A patch is written as core release/1.0.27's c4 diff
// writes it (cmd/c4/diff.go: the old state's C4 ID, then the encoded
// patch): the entries, then the lists of the ranges it adds or changes.
// EncodePatch writes the same: through 1.0.26 it then wrote the new
// state's C4 ID, which a c4m file never ends with (2026-11-08).

static std::string asDiffWrites(const c4m::PatchResult &pr) {
    return pr.oldID.String() + "\n" + pr.patch.Encode();
}

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

TEST_CASE("range data vector: diff plain.c4m to folded.c4m is add.c4m, both lists after the entries",
          "[c4m][vector][sequence][range-data][diff]") {
    auto pr = c4m::PatchDiff(parse(RD, "plain.c4m"), parse(RD, "folded.c4m"));
    CHECK(pr.patch.RangeData().size() == 2);
    CHECK(asDiffWrites(pr) == readAll(RD, "add.c4m"));
    CHECK(c4m::EncodePatch(pr) == readAll(RD, "add.c4m"));
}

TEST_CASE("range data vector: diff folded.c4m to no-plate.c4m is remove.c4m, a removal takes no list",
          "[c4m][vector][sequence][range-data][diff]") {
    auto pr = c4m::PatchDiff(parse(RD, "folded.c4m"), parse(RD, "no-plate.c4m"));
    CHECK(pr.patch.RangeData().empty());
    CHECK(asDiffWrites(pr) == readAll(RD, "remove.c4m"));
    CHECK(c4m::EncodePatch(pr) == readAll(RD, "remove.c4m"));
}

TEST_CASE("range data vector: DecodePatchChain and ResolvePatchChain resolve both chains with their lists",
          "[c4m][vector][sequence][range-data][chain]") {
    // add-chain.c4m carries its lists in the patch section, remove-chain.c4m
    // in the base section, before the checkpoint.
    auto added = c4m::ResolvePatchChain(c4m::DecodePatchChain(std::string_view(readAll(RD, "add-chain.c4m"))));
    CHECK(added.RangeData().size() == 2);
    CHECK(added.Encode() == readAll(RD, "folded.c4m"));
    auto removed = c4m::ResolvePatchChain(c4m::DecodePatchChain(std::string_view(readAll(RD, "remove-chain.c4m"))));
    CHECK(removed.GetEntry("plate.[1001-1004].exr") == nullptr);
    CHECK(removed.RangeData().size() == 1);
    CHECK(removed.Encode() == readAll(RD, "no-plate.c4m"));
}

TEST_CASE("range data vector: DecodePatchChain keeps each section's lists with that section",
          "[c4m][vector][sequence][range-data][chain]") {
    auto add = c4m::DecodePatchChain(std::string_view(readAll(RD, "add-chain.c4m")));
    REQUIRE(add.size() == 2);
    CHECK(add[0].rangeData.empty());
    CHECK(add[1].rangeData.size() == 2);
    auto rm = c4m::DecodePatchChain(std::string_view(readAll(RD, "remove-chain.c4m")));
    REQUIRE(rm.size() == 2);
    CHECK(rm[0].rangeData.size() == 2);
    CHECK(rm[1].rangeData.empty());
    // Resolving only the base section keeps both of its lists.
    CHECK(c4m::ResolvePatchChain(rm, 1).Encode() == readAll(RD, "folded.c4m"));
}

// ---- Filters build a c4m from a c4m, so they carry the lists too ----
//
// The vector has no filter row; these hold FilterByPath and FilterByPrefix
// to the same rule over the vector's files, as Go's do.

TEST_CASE("range data vector: FilterByPath and FilterByPrefix carry the lists of the ranges they keep",
          "[c4m][vector][sequence][range-data][filter]") {
    const auto folded = parse(RD, "folded.c4m");
    CHECK(folded.FilterByPrefix("").Encode() == readAll(RD, "folded.c4m"));
    const c4m::Entry *shot = find(folded, "shot.[001-003].exr");
    REQUIRE(shot != nullptr);
    auto sub = folded.FilterByPrefix("sub/");
    REQUIRE(sub.RangeData().size() == 1);
    CHECK(sub.RangeData().begin()->first == shot->id);
    auto plate = folded.FilterByPath("plate.*");
    REQUIRE(plate.EntryCount() == 1);
    REQUIRE(plate.RangeData().size() == 1);
    CHECK(plate.RangeData().begin()->first == find(folded, "plate.[1001-1004].exr")->id);
    CHECK(folded.FilterByPath("*.txt").RangeData().empty());
}
