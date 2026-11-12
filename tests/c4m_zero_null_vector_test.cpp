// SPDX-License-Identifier: Apache-2.0
// Cross-implementation known-zero-against-null conformance test
// (2026-11-09). The fixtures under testdata/zero-null-vector/ are
// byte-identical copies of oss/c4/c4m/testdata/zero-null-vector/, and
// testdata/legacy/ten-dash-null-1.0.21.c4m is the real file c4 1.0.21
// wrote. See zero-null-vector/README.md, SPECIFICATION.md "Null and Zero
// Values" and C4M-STANDARD 4.1.
//
//   "-"           null mode: not yet checked
//   "----------"  known zero: a regular file with zero permissions
//   "d---------"  known: a directory with zero permissions
//
// Both canonical.c4m and pretty.c4m MUST decode to one manifest whose
// root ID is root-id.txt. Ten dashes on a directory line is the one
// legacy exception: old pretty encoders wrote null that way, so it
// decodes as null.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_ZERO_NULL_VECTOR_DIR
#error "C4M_ZERO_NULL_VECTOR_DIR must be defined by the build"
#endif
#ifndef C4M_LEGACY_DIR
#error "C4M_LEGACY_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_ZERO_NULL_VECTOR_DIR) / name;
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

const char *kEmptyID =
    "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdZrAkZzsWcbWtDg5oQstpDuni4Hirj75GEmTc1sFT";

const c4m::Entry &entry(const c4m::Manifest &m, const std::string &path) {
    const c4m::Entry *e = m.GetEntry(path);
    REQUIRE(e != nullptr);
    return *e;
}

// The per-entry contract from the vector's README.
void checkVector(const c4m::Manifest &m) {
    REQUIRE(m.EntryCount() == 7);

    const auto &zero = entry(m, "zero.txt");
    REQUIRE(zero.mode == c4m::ZeroMode);
    REQUIRE_FALSE(c4m::IsNullMode(zero.mode));
    REQUIRE(zero.size == 4);
    REQUIRE_FALSE(zero.id.IsNil());

    const auto &nullmode = entry(m, "nullmode.txt");
    REQUIRE(nullmode.mode == 0);
    REQUIRE(c4m::IsNullMode(nullmode.mode));
    REQUIRE(nullmode.size == 4);
    REQUIRE_FALSE(nullmode.id.IsNil());

    const auto &empty = entry(m, "empty.txt");
    REQUIRE(empty.mode == 0644);
    REQUIRE(empty.size == 0);
    REQUIRE(empty.id == c4::ID::Parse(kEmptyID));

    const auto &emptydir = entry(m, "emptydir/");
    REQUIRE(emptydir.mode == (c4m::ModeDir | 0755));
    REQUIRE(emptydir.size == 0);
    REQUIRE(emptydir.id == c4::ID::Parse(kEmptyID));

    const auto &locked = entry(m, "locked/");
    REQUIRE(locked.mode == c4m::ModeDir);
    REQUIRE_FALSE(c4m::IsNullMode(locked.mode));
    REQUIRE(locked.IsDir());
    REQUIRE(locked.size == 142);
    REQUIRE_FALSE(locked.id.IsNil());
    REQUIRE(m.Children(&locked).size() == 1);

    const auto &inside = entry(m, "locked/inside.txt");
    REQUIRE(inside.mode == 0644);
    REQUIRE(inside.size == 6);
    REQUIRE_FALSE(inside.id.IsNil());

    const auto &unread = entry(m, "unread/");
    REQUIRE(unread.mode == c4m::ModeDir);
    REQUIRE_FALSE(c4m::IsNullMode(unread.mode));
    REQUIRE(unread.size == c4m::NullSize);
    REQUIRE(unread.id.IsNil());
    REQUIRE(m.Children(&unread).empty());

    REQUIRE(m.ComputeC4ID() == c4::ID::Parse(readTrimmed(fixture("root-id.txt"))));
}

} // namespace

TEST_CASE("zero-null-vector: canonical.c4m decodes to the pinned manifest", "[c4m][vector][zero-null]") {
    c4m::Manifest m;
    REQUIRE_NOTHROW(m = c4m::Manifest::ParseFile(fixture("canonical.c4m")));
    checkVector(m);
}

TEST_CASE("zero-null-vector: canonical.c4m re-encodes byte-for-byte", "[c4m][vector][zero-null]") {
    std::string text = readAll(fixture("canonical.c4m"));
    c4m::Manifest m = c4m::Manifest::Parse(text);
    REQUIRE(m.Encode() == text);

    // And the canonical line of every entry is the line it was read from.
    std::string canonical;
    for (const auto &e : m.Entries()) {
        canonical += std::string(static_cast<size_t>(e.depth) * 2, ' ');
        canonical += e.Canonical();
        canonical += '\n';
    }
    REQUIRE(canonical == text);
}

TEST_CASE("zero-null-vector: pretty.c4m decodes to the same manifest", "[c4m][vector][zero-null]") {
    c4m::Manifest pretty;
    REQUIRE_NOTHROW(pretty = c4m::Manifest::ParseFile(fixture("pretty.c4m")));
    checkVector(pretty);

    // Field by field the same as the canonical decode, and the pretty
    // form re-encodes to the canonical text (libc4 has one encoder form:
    // ISO-8601 UTC timestamps, two-space indentation).
    c4m::Manifest canonical = c4m::Manifest::ParseFile(fixture("canonical.c4m"));
    REQUIRE(pretty.Encode() == canonical.Encode());
    REQUIRE(pretty.Encode() == readAll(fixture("canonical.c4m")));
}

TEST_CASE("zero-null-vector: the two forms normalise to one text", "[c4m][vector][zero-null]") {
    // Both forms decode to one description, with one description ID, and
    // the pretty form re-encodes to the canonical text. Each file keeps its
    // own byte ID (a file's C4 ID is its bytes).
    auto canonical = c4m::Manifest::Parse(readAll(fixture("canonical.c4m")));
    auto pretty = c4m::Manifest::Parse(readAll(fixture("pretty.c4m")));
    REQUIRE(canonical.ComputeC4ID() == pretty.ComputeC4ID());
    REQUIRE(pretty.Encode() == readAll(fixture("canonical.c4m")));
}

TEST_CASE("legacy: ten dashes on every directory of a 1.0.21 file decodes as null", "[c4m][vector][zero-null][legacy]") {
    std::filesystem::path p = std::filesystem::path(C4M_LEGACY_DIR) / "ten-dash-null-1.0.21.c4m";
    c4m::Manifest m;
    REQUIRE_NOTHROW(m = c4m::Manifest::ParseFile(p));
    REQUIRE(m.EntryCount() == 3);

    // Every directory in the file was written by an encoder that spelled
    // null as ten dashes; a zero-permission directory would be "d---------".
    for (const c4m::Entry *e : m.AllEntries()) {
        if (!e->IsDir())
            continue;
        INFO("directory " << m.EntryPath(e));
        REQUIRE(e->mode == 0);
        REQUIRE(c4m::IsNullMode(e->mode));
        REQUIRE(e->HasNullValues());
    }
    REQUIRE(entry(m, "d/").mode == 0);

    // Ten dashes on a file line is the known zero, legacy file or not.
    REQUIRE(entry(m, "a.txt").mode == c4m::ZeroMode);
    REQUIRE(entry(m, "d/b.txt").mode == c4m::ZeroMode);

    // Re-encoded, the directory is null ("-") and the files stay zero.
    REQUIRE(m.Encode() ==
            "---------- - - a.txt -\n"
            "- - - d/ -\n"
            "  ---------- - - b.txt -\n");
}
