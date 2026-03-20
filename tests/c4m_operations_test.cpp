// SPDX-License-Identifier: Apache-2.0
// Tests for c4m operations: PatchDiff, ApplyPatch, Diff, EntryPaths,
// EncodePatch, DecodePatchChain, ResolvePatchChain, Merge.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

// Helper: create a file entry with common defaults.
static c4m::Entry makeFile(const std::string &name, int64_t size,
                            int64_t ts = 1704067200, uint32_t mode = 0644) {
    c4m::Entry e;
    e.mode = mode;
    e.timestamp = ts;
    e.size = size;
    e.name = name;
    e.id = c4::ID::Identify(name + std::to_string(size));
    return e;
}

// Helper: create a directory entry.
static c4m::Entry makeDir(const std::string &name, int64_t ts = 1704067200) {
    c4m::Entry e;
    e.mode = c4m::ModeDir | 0755;
    e.timestamp = ts;
    e.size = 0;
    e.name = name;
    return e;
}

// =============================================================
// EntryPaths
// =============================================================

TEST_CASE("EntryPaths: flat file list", "[c4m][ops]") {
    c4m::Manifest m;
    m.AddEntry(makeFile("a.txt", 100));
    m.AddEntry(makeFile("b.txt", 200));

    auto paths = c4m::EntryPaths(m.Entries());
    REQUIRE(paths.size() == 2);
    REQUIRE(paths.count("a.txt") == 1);
    REQUIRE(paths.count("b.txt") == 1);
}

TEST_CASE("EntryPaths: nested entries", "[c4m][ops]") {
    c4m::Manifest m;
    auto dir = makeDir("src/");
    dir.depth = 0;
    m.AddEntry(dir);

    auto file = makeFile("main.cpp", 512);
    file.depth = 1;
    m.AddEntry(file);

    auto paths = c4m::EntryPaths(m.Entries());
    REQUIRE(paths.size() == 2);
    REQUIRE(paths.count("src/") == 1);
    REQUIRE(paths.count("src/main.cpp") == 1);
}

// =============================================================
// Diff
// =============================================================

TEST_CASE("Diff: identical manifests", "[c4m][ops]") {
    c4m::Manifest m;
    m.AddEntry(makeFile("a.txt", 100));
    m.AddEntry(makeFile("b.txt", 200));

    auto result = c4m::Diff(m, m);
    REQUIRE(result.IsEmpty());
    REQUIRE(result.same.EntryCount() == 2);
}

TEST_CASE("Diff: addition", "[c4m][ops]") {
    c4m::Manifest a, b;
    a.AddEntry(makeFile("a.txt", 100));
    b.AddEntry(makeFile("a.txt", 100));
    b.AddEntry(makeFile("new.txt", 300));

    auto result = c4m::Diff(a, b);
    REQUIRE(result.added.EntryCount() == 1);
    REQUIRE(result.added.GetEntry("new.txt") != nullptr);
    REQUIRE(result.removed.EntryCount() == 0);
}

TEST_CASE("Diff: removal", "[c4m][ops]") {
    c4m::Manifest a, b;
    a.AddEntry(makeFile("a.txt", 100));
    a.AddEntry(makeFile("old.txt", 200));
    b.AddEntry(makeFile("a.txt", 100));

    auto result = c4m::Diff(a, b);
    REQUIRE(result.removed.EntryCount() == 1);
    REQUIRE(result.removed.GetEntry("old.txt") != nullptr);
}

TEST_CASE("Diff: modification", "[c4m][ops]") {
    c4m::Manifest a, b;
    a.AddEntry(makeFile("a.txt", 100));
    b.AddEntry(makeFile("a.txt", 999)); // different size -> different ID

    auto result = c4m::Diff(a, b);
    REQUIRE(result.modified.EntryCount() == 1);
}

// =============================================================
// PatchDiff + ApplyPatch
// =============================================================

TEST_CASE("PatchDiff: no changes yields empty patch", "[c4m][ops]") {
    c4m::Manifest m;
    m.AddEntry(makeFile("a.txt", 100));

    auto pr = c4m::PatchDiff(m, m);
    REQUIRE(pr.IsEmpty());
}

TEST_CASE("PatchDiff: addition", "[c4m][ops]") {
    c4m::Manifest old_m, new_m;
    old_m.AddEntry(makeFile("a.txt", 100));
    new_m.AddEntry(makeFile("a.txt", 100));
    new_m.AddEntry(makeFile("b.txt", 200));

    auto pr = c4m::PatchDiff(old_m, new_m);
    REQUIRE_FALSE(pr.IsEmpty());
    REQUIRE(pr.patch.EntryCount() == 1);
    REQUIRE(pr.patch.GetEntry("b.txt") != nullptr);
}

TEST_CASE("PatchDiff: removal", "[c4m][ops]") {
    c4m::Manifest old_m, new_m;
    old_m.AddEntry(makeFile("a.txt", 100));
    old_m.AddEntry(makeFile("b.txt", 200));
    new_m.AddEntry(makeFile("a.txt", 100));

    auto pr = c4m::PatchDiff(old_m, new_m);
    REQUIRE_FALSE(pr.IsEmpty());
    // Removal emits exact copy of old entry
    REQUIRE(pr.patch.EntryCount() == 1);
    REQUIRE(pr.patch.GetEntry("b.txt") != nullptr);
}

TEST_CASE("PatchDiff: modification", "[c4m][ops]") {
    c4m::Manifest old_m, new_m;
    old_m.AddEntry(makeFile("a.txt", 100));
    new_m.AddEntry(makeFile("a.txt", 999)); // changed

    auto pr = c4m::PatchDiff(old_m, new_m);
    REQUIRE_FALSE(pr.IsEmpty());
    REQUIRE(pr.patch.EntryCount() == 1);
}

TEST_CASE("ApplyPatch: addition", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));

    c4m::Manifest patch;
    patch.AddEntry(makeFile("b.txt", 200));

    auto result = c4m::ApplyPatch(base, patch);
    REQUIRE(result.EntryCount() == 2);
    REQUIRE(result.GetEntry("a.txt") != nullptr);
    REQUIRE(result.GetEntry("b.txt") != nullptr);
}

TEST_CASE("ApplyPatch: removal by exact duplicate", "[c4m][ops]") {
    auto file_a = makeFile("a.txt", 100);
    auto file_b = makeFile("b.txt", 200);

    c4m::Manifest base;
    base.AddEntry(file_a);
    base.AddEntry(file_b);

    // Patch contains exact copy of b.txt -> signals removal
    c4m::Manifest patch;
    patch.AddEntry(file_b);

    auto result = c4m::ApplyPatch(base, patch);
    REQUIRE(result.EntryCount() == 1);
    REQUIRE(result.GetEntry("a.txt") != nullptr);
    REQUIRE(result.GetEntry("b.txt") == nullptr);
}

TEST_CASE("ApplyPatch: modification by clobber", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));

    auto modified = makeFile("a.txt", 999);
    c4m::Manifest patch;
    patch.AddEntry(modified);

    auto result = c4m::ApplyPatch(base, patch);
    REQUIRE(result.EntryCount() == 1);
    auto *e = result.GetEntry("a.txt");
    REQUIRE(e != nullptr);
    REQUIRE(e->size == 999);
}

TEST_CASE("PatchDiff + ApplyPatch round-trip", "[c4m][ops]") {
    c4m::Manifest old_m, new_m;
    old_m.AddEntry(makeFile("a.txt", 100));
    old_m.AddEntry(makeFile("b.txt", 200));
    old_m.AddEntry(makeFile("c.txt", 300));

    new_m.AddEntry(makeFile("a.txt", 100));  // unchanged
    new_m.AddEntry(makeFile("b.txt", 999));  // modified
    new_m.AddEntry(makeFile("d.txt", 400));  // added (c removed)

    auto pr = c4m::PatchDiff(old_m, new_m);
    REQUIRE_FALSE(pr.IsEmpty());

    auto result = c4m::ApplyPatch(old_m, pr.patch);
    REQUIRE(result.EntryCount() == 3);
    REQUIRE(result.GetEntry("a.txt") != nullptr);
    REQUIRE(result.GetEntry("b.txt") != nullptr);
    REQUIRE(result.GetEntry("b.txt")->size == 999);
    REQUIRE(result.GetEntry("d.txt") != nullptr);
    REQUIRE(result.GetEntry("c.txt") == nullptr);
}

TEST_CASE("PatchDiff + ApplyPatch with directories", "[c4m][ops]") {
    c4m::Manifest old_m, new_m;

    // In C4, directory C4IDs change when children change. Simulate this
    // by giving the old and new dir entries different C4IDs.
    auto old_dir = makeDir("src/");
    old_dir.depth = 0;
    old_dir.id = c4::ID::Identify("old-src-tree");

    auto new_dir = makeDir("src/");
    new_dir.depth = 0;
    new_dir.id = c4::ID::Identify("new-src-tree");

    auto file1 = makeFile("main.cpp", 512);
    file1.depth = 1;

    old_m.AddEntry(old_dir);
    old_m.AddEntry(file1);
    old_m.AddEntry(makeFile("readme.txt", 100));

    new_m.AddEntry(new_dir);
    new_m.AddEntry(file1);
    auto file2 = makeFile("util.cpp", 256);
    file2.depth = 1;
    new_m.AddEntry(file2);
    new_m.AddEntry(makeFile("readme.txt", 999));

    auto pr = c4m::PatchDiff(old_m, new_m);
    REQUIRE_FALSE(pr.IsEmpty());

    auto result = c4m::ApplyPatch(old_m, pr.patch);
    auto paths = c4m::EntryPaths(result.Entries());
    REQUIRE(paths.count("src/main.cpp") == 1);
    REQUIRE(paths.count("src/util.cpp") == 1);
    REQUIRE(paths.count("readme.txt") == 1);
    REQUIRE(paths["readme.txt"]->size == 999);
}

// =============================================================
// EncodePatch
// =============================================================

TEST_CASE("EncodePatch: basic structure", "[c4m][ops]") {
    c4m::Manifest old_m, new_m;
    old_m.AddEntry(makeFile("a.txt", 100));
    new_m.AddEntry(makeFile("a.txt", 100));
    new_m.AddEntry(makeFile("b.txt", 200));

    auto pr = c4m::PatchDiff(old_m, new_m);
    std::string encoded = c4m::EncodePatch(pr);

    // Should contain old ID, entries, new ID
    REQUIRE_FALSE(encoded.empty());
    REQUIRE(encoded.find(pr.oldID.String()) != std::string::npos);
    REQUIRE(encoded.find(pr.newID.String()) != std::string::npos);
    REQUIRE(encoded.find("b.txt") != std::string::npos);
}

// =============================================================
// DecodePatchChain + ResolvePatchChain
// =============================================================

TEST_CASE("DecodePatchChain: single section", "[c4m][ops]") {
    // A simple manifest text (no bare C4 ID lines = single section)
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 a.txt -\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 200 b.txt -\n";

    auto sections = c4m::DecodePatchChain(std::string_view(input));
    REQUIRE(sections.size() == 1);
    REQUIRE(sections[0].entries.size() == 2);
    REQUIRE(sections[0].baseID.IsNil());
}

TEST_CASE("DecodePatchChain: two sections with bare ID separator", "[c4m][ops]") {
    auto id = c4::ID::Identify("test boundary");

    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 base.txt -\n"
        + id.String() + "\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 200 patch.txt -\n";

    auto sections = c4m::DecodePatchChain(std::string_view(input));
    REQUIRE(sections.size() == 2);
    REQUIRE(sections[0].entries.size() == 1);
    REQUIRE(sections[0].entries[0].name == "base.txt");
    REQUIRE(sections[1].baseID == id);
    REQUIRE(sections[1].entries.size() == 1);
    REQUIRE(sections[1].entries[0].name == "patch.txt");
}

TEST_CASE("DecodePatchChain: skips inline ID lists", "[c4m][ops]") {
    auto id1 = c4::ID::Identify("content1");
    auto id2 = c4::ID::Identify("content2");
    // Inline ID list: two IDs concatenated (180 chars, multiple of 90)
    std::string inline_list = id1.String() + id2.String();
    REQUIRE(inline_list.size() == 180);

    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt -\n"
        + inline_list + "\n";

    auto sections = c4m::DecodePatchChain(std::string_view(input));
    REQUIRE(sections.size() == 1);
    REQUIRE(sections[0].entries.size() == 1);
}

TEST_CASE("ResolvePatchChain: single section", "[c4m][ops]") {
    c4m::PatchSection sec;
    sec.entries.push_back(makeFile("a.txt", 100));
    sec.entries.push_back(makeFile("b.txt", 200));

    std::vector<c4m::PatchSection> sections = {sec};
    auto result = c4m::ResolvePatchChain(sections);
    REQUIRE(result.EntryCount() == 2);
}

TEST_CASE("ResolvePatchChain: base + patch", "[c4m][ops]") {
    // Base section
    c4m::PatchSection base_sec;
    base_sec.entries.push_back(makeFile("a.txt", 100));
    base_sec.entries.push_back(makeFile("b.txt", 200));

    // Patch section: add c.txt
    c4m::PatchSection patch_sec;
    patch_sec.entries.push_back(makeFile("c.txt", 300));

    std::vector<c4m::PatchSection> sections = {base_sec, patch_sec};
    auto result = c4m::ResolvePatchChain(sections);
    REQUIRE(result.EntryCount() == 3);
    REQUIRE(result.GetEntry("a.txt") != nullptr);
    REQUIRE(result.GetEntry("b.txt") != nullptr);
    REQUIRE(result.GetEntry("c.txt") != nullptr);
}

TEST_CASE("ResolvePatchChain: stopAt limits sections", "[c4m][ops]") {
    c4m::PatchSection sec1;
    sec1.entries.push_back(makeFile("a.txt", 100));

    c4m::PatchSection sec2;
    sec2.entries.push_back(makeFile("b.txt", 200));

    c4m::PatchSection sec3;
    sec3.entries.push_back(makeFile("c.txt", 300));

    std::vector<c4m::PatchSection> sections = {sec1, sec2, sec3};

    auto r1 = c4m::ResolvePatchChain(sections, 1);
    REQUIRE(r1.EntryCount() == 1);

    auto r2 = c4m::ResolvePatchChain(sections, 2);
    REQUIRE(r2.EntryCount() == 2);
}

// =============================================================
// Merge (three-way)
// =============================================================

TEST_CASE("Merge: no changes", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));

    auto result = c4m::Merge(base, base, base);
    REQUIRE(result.conflicts.empty());
    REQUIRE(result.merged.EntryCount() == 1);
}

TEST_CASE("Merge: local addition", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));

    c4m::Manifest local;
    local.AddEntry(makeFile("a.txt", 100));
    local.AddEntry(makeFile("local.txt", 200));

    auto result = c4m::Merge(base, local, base);
    REQUIRE(result.conflicts.empty());
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("a.txt") == 1);
    REQUIRE(paths.count("local.txt") == 1);
}

TEST_CASE("Merge: remote addition", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));

    c4m::Manifest remote;
    remote.AddEntry(makeFile("a.txt", 100));
    remote.AddEntry(makeFile("remote.txt", 300));

    auto result = c4m::Merge(base, base, remote);
    REQUIRE(result.conflicts.empty());
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("remote.txt") == 1);
}

TEST_CASE("Merge: both add different files (no conflict)", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));

    c4m::Manifest local;
    local.AddEntry(makeFile("a.txt", 100));
    local.AddEntry(makeFile("local.txt", 200));

    c4m::Manifest remote;
    remote.AddEntry(makeFile("a.txt", 100));
    remote.AddEntry(makeFile("remote.txt", 300));

    auto result = c4m::Merge(base, local, remote);
    REQUIRE(result.conflicts.empty());
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("a.txt") == 1);
    REQUIRE(paths.count("local.txt") == 1);
    REQUIRE(paths.count("remote.txt") == 1);
}

TEST_CASE("Merge: both modify same file (conflict, LWW)", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100, 1000));

    // Local modifies at ts=2000
    auto local_file = makeFile("a.txt", 200, 2000);
    c4m::Manifest local;
    local.AddEntry(local_file);

    // Remote modifies at ts=3000 (newer)
    auto remote_file = makeFile("a.txt", 300, 3000);
    c4m::Manifest remote;
    remote.AddEntry(remote_file);

    auto result = c4m::Merge(base, local, remote);
    REQUIRE(result.conflicts.size() == 1);
    REQUIRE(result.conflicts[0].path == "a.txt");

    // Remote wins (newer timestamp)
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("a.txt") == 1);
    REQUIRE(paths["a.txt"]->size == 300);

    // Loser gets .conflict suffix
    REQUIRE(paths.count("a.txt.conflict") == 1);
    REQUIRE(paths["a.txt.conflict"]->size == 200);
}

TEST_CASE("Merge: local deletes, remote unchanged -> delete", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));
    base.AddEntry(makeFile("b.txt", 200));

    c4m::Manifest local;
    local.AddEntry(makeFile("a.txt", 100));
    // b.txt deleted locally

    auto result = c4m::Merge(base, local, base);
    REQUIRE(result.conflicts.empty());
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("a.txt") == 1);
    REQUIRE(paths.count("b.txt") == 0);
}

TEST_CASE("Merge: both delete same file -> agreed deletion", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100));
    base.AddEntry(makeFile("b.txt", 200));

    c4m::Manifest local;
    local.AddEntry(makeFile("a.txt", 100));

    c4m::Manifest remote;
    remote.AddEntry(makeFile("a.txt", 100));

    auto result = c4m::Merge(base, local, remote);
    REQUIRE(result.conflicts.empty());
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("b.txt") == 0);
}

TEST_CASE("Merge: local modifies, remote deletes -> conflict", "[c4m][ops]") {
    c4m::Manifest base;
    base.AddEntry(makeFile("a.txt", 100, 1000));

    c4m::Manifest local;
    local.AddEntry(makeFile("a.txt", 200, 2000)); // modified

    c4m::Manifest remote;
    // a.txt deleted

    auto result = c4m::Merge(base, local, remote);
    REQUIRE(result.conflicts.size() == 1);
    REQUIRE(result.conflicts[0].path == "a.txt");
    REQUIRE(result.conflicts[0].remoteDeleted);
}

TEST_CASE("Merge: both add same file identically -> no conflict", "[c4m][ops]") {
    c4m::Manifest base;

    auto file = makeFile("new.txt", 100);
    c4m::Manifest local;
    local.AddEntry(file);

    c4m::Manifest remote;
    remote.AddEntry(file);

    auto result = c4m::Merge(base, local, remote);
    REQUIRE(result.conflicts.empty());
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("new.txt") == 1);
}

TEST_CASE("Merge: empty base (first sync)", "[c4m][ops]") {
    c4m::Manifest base; // empty

    c4m::Manifest local;
    local.AddEntry(makeFile("a.txt", 100));

    c4m::Manifest remote;
    remote.AddEntry(makeFile("b.txt", 200));

    auto result = c4m::Merge(base, local, remote);
    REQUIRE(result.conflicts.empty());
    auto paths = c4m::EntryPaths(result.merged.Entries());
    REQUIRE(paths.count("a.txt") == 1);
    REQUIRE(paths.count("b.txt") == 1);
}

// =============================================================
// Integration: EncodePatch -> DecodePatchChain -> ResolvePatchChain
// =============================================================

TEST_CASE("Patch chain encode-decode round-trip", "[c4m][ops]") {
    c4m::Manifest old_m, new_m;
    old_m.AddEntry(makeFile("a.txt", 100));
    old_m.AddEntry(makeFile("b.txt", 200));

    new_m.AddEntry(makeFile("a.txt", 100));
    new_m.AddEntry(makeFile("c.txt", 300));

    auto pr = c4m::PatchDiff(old_m, new_m);
    std::string encoded = c4m::EncodePatch(pr);

    // Decode into chain sections
    auto sections = c4m::DecodePatchChain(std::string_view(encoded));
    // Should have 1 section (the patch entries between the two ID boundaries)
    REQUIRE(sections.size() >= 1);
}
