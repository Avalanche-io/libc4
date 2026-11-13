// SPDX-License-Identifier: Apache-2.0
// A size of 0 beside a C4 ID other than the empty content's is a null size
// (ruled 2026-11-08; c4 #68, landed in 1.1 with every port following,
// decided 2026-11-13 by the release lead; reversible). A filesystem surface
// has no "-", so a null size shows there as 0, and only the empty content
// truly has size 0. A 0 with no C4 ID, a 0 with the empty content's ID, and
// a symlink's 0 are known zeros.
//
// The fixtures under testdata/null-size-vector/ are byte-identical copies of
// oss/c4/c4m/testdata/null-size-vector/. Both canonical.c4m and
// zero-spelled.c4m MUST decode to root-id.txt, see each entry's size as the
// README table says, and re-encode to canonical.c4m byte for byte.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef C4M_NULL_SIZE_VECTOR_DIR
#error "C4M_NULL_SIZE_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_NULL_SIZE_VECTOR_DIR) / name;
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

c4::ID idOf(const std::string &s) { return c4::ID::Identify(std::string_view(s)); }

const int64_t kTS = c4m::ParseTimestamp("2026-03-06T12:00:00Z");

c4m::Entry makeEntry(const std::string &name, uint32_t mode, int64_t size, const c4::ID &id,
                     const std::string &target = "") {
    c4m::Entry e;
    e.mode = mode;
    e.timestamp = kTS;
    e.size = size;
    e.name = name;
    e.id = id;
    e.target = target;
    return e;
}

// The description the vector pins, built through the API exactly as Go's
// nullSizeVectorManifest builds it: every way a size of 0 can stand beside
// a C4 ID.
c4m::Manifest vectorManifest() {
    c4m::Manifest m;
    m.AddEntry(makeEntry("null.txt", 0644, -1, idOf("hello")));
    m.AddEntry(makeEntry("empty.txt", 0644, 0, c4m::EmptyContentID()));
    m.AddEntry(makeEntry("structure.txt", 0644, 0, c4::ID()));
    m.AddEntry(makeEntry("known.txt", 0644, 5, idOf("hello")));
    m.AddEntry(makeEntry("link", c4m::ModeSymlink | 0755, 0, c4::ID(), "known.txt"));
    m.AddEntry(makeEntry("emptydir/", c4m::ModeDir | 0755, 0, c4m::EmptyContentID()));
    m.AddEntry(makeEntry("collapsed/", c4m::ModeDir | 0755, -1, idOf("a listing")));
    return m;
}

} // namespace

TEST_CASE("null-size vector: EmptyContentID is the ID of zero bytes", "[c4m][vector][null-size]") {
    REQUIRE(c4m::EmptyContentID().String() ==
            "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdZrAkZzsWcbWtDg5oQstpDuni4Hirj75GEmTc1sFT");
}

TEST_CASE("null-size vector: the library writes canonical.c4m and root-id.txt", "[c4m][vector][null-size]") {
    auto m = vectorManifest();
    REQUIRE(m.Encode() == readAll(fixture("canonical.c4m")));
    REQUIRE(m.ComputeC4ID().String() == readTrimmed(fixture("root-id.txt")));
}

TEST_CASE("null-size vector: both files decode to one manifest and re-encode canonically",
          "[c4m][vector][null-size]") {
    const std::string root = readTrimmed(fixture("root-id.txt"));
    const std::string canonical = readAll(fixture("canonical.c4m"));
    const std::map<std::string, bool> wantNull = {
        {"null.txt", true},       {"empty.txt", false}, {"structure.txt", false},
        {"known.txt", false},     {"link", false},      {"emptydir/", false},
        {"collapsed/", true},
    };
    for (const char *name : {"canonical.c4m", "zero-spelled.c4m"}) {
        INFO(name);
        auto m = c4m::Manifest::Parse(std::string_view(readAll(fixture(name))));
        CHECK(m.ComputeC4ID().String() == root);
        REQUIRE(m.EntryCount() == wantNull.size());
        for (const auto &e : m.Entries()) {
            INFO(e.name);
            auto it = wantNull.find(e.name);
            REQUIRE(it != wantNull.end());
            CHECK(e.SizeIsNull() == it->second);
            // The decoder holds a null size as -1; a known zero stays 0.
            if (it->second)
                CHECK(e.size == c4m::NullSize);
            else
                CHECK(e.size == (e.name == "known.txt" ? 5 : 0));
        }
        CHECK(m.Encode() == canonical);
    }
}

TEST_CASE("null size: each way a 0 can stand beside a C4 ID", "[c4m][null-size]") {
    const std::string ts = "2026-03-06T12:00:00Z";
    const std::string hello = idOf("hello").String();
    const std::string empty = c4m::EmptyContentID().String();
    struct Case {
        const char *what;
        std::string line;
        bool wantNull;
    };
    const std::vector<Case> cases = {
        {"zero with real content ID is null", "-rw-r--r-- " + ts + " 0 f " + hello, true},
        {"zero with empty-content ID is known", "-rw-r--r-- " + ts + " 0 f " + empty, false},
        {"zero with no ID is known (structure scan)", "-rw-r--r-- " + ts + " 0 f -", false},
        {"symlink zero is its own size", "lrwxr-xr-x " + ts + " 0 l -> f " + hello, false},
        {"directory zero with listing ID is known", "drwxr-xr-x " + ts + " 0 d/ " + empty, false},
        {"directory zero with other ID is null", "drwxr-xr-x " + ts + " 0 d/ " + hello, true},
        {"non-zero is known", "-rw-r--r-- " + ts + " 5 f " + hello, false},
        {"dash is null", "-rw-r--r-- " + ts + " - f " + hello, true},
    };
    for (const auto &c : cases) {
        INFO(c.what);
        auto m = c4m::Manifest::Parse(std::string_view(c.line + "\n"));
        REQUIRE(m.EntryCount() == 1);
        const auto &e = m.Entries()[0];
        CHECK(e.SizeIsNull() == c.wantNull);
        if (c.wantNull)
            CHECK(e.size == c4m::NullSize);
        // The canonical text writes the null as "-", a known size as itself.
        std::istringstream fields(e.Canonical());
        std::string mode, stamp, size;
        fields >> mode >> stamp >> size;
        CHECK((size == "-") == c.wantNull);
    }
}

TEST_CASE("null size: 0 and - beside a real content ID are one description", "[c4m][null-size]") {
    const auto id = idOf("hello");
    auto fromZero = c4m::Manifest::Parse(
        std::string_view("-rw-r--r-- 2026-03-06T12:00:00Z 0 f " + id.String() + "\n"));
    auto fromDash = c4m::Manifest::Parse(
        std::string_view("-rw-r--r-- 2026-03-06T12:00:00Z - f " + id.String() + "\n"));
    REQUIRE(fromZero.ComputeC4ID() == fromDash.ComputeC4ID());

    // Built through the API with size 0 (as a scan over a mount would
    // build it), the entry still renders and hashes as null.
    c4m::Manifest built;
    built.AddEntry(makeEntry("f", 0644, 0, id));
    REQUIRE(built.ComputeC4ID() == fromDash.ComputeC4ID());
    REQUIRE(built.Encode().find(" - f ") != std::string::npos);
    REQUIRE(built.Entries()[0].HasNullValues());
}

TEST_CASE("null size: a null written as 0 makes its parent's size null", "[c4m][null-size]") {
    const auto id = idOf("hello");
    auto m = c4m::Manifest::Parse(std::string_view(
        "drwxr-xr-x 2026-03-06T12:00:00Z - d/ -\n"
        "  -rw-r--r-- 2026-03-06T12:00:00Z 0 f " + id.String() + "\n"));

    // The same tree with the child's size known gives the directory a size,
    // so the null below is the child's doing, not the directory's.
    auto known = c4m::Manifest::Parse(std::string_view(
        "drwxr-xr-x 2026-03-06T12:00:00Z - d/ -\n"
        "  -rw-r--r-- 2026-03-06T12:00:00Z 5 f " + id.String() + "\n"));

    m.Canonicalize();
    known.Canonicalize();
    REQUIRE(m.GetEntry("d/")->SizeIsNull());
    REQUIRE_FALSE(known.GetEntry("d/")->SizeIsNull());

    // An API-built child with size 0 beside a real ID infects the parent too.
    c4m::Manifest built;
    auto dir = makeEntry("d/", c4m::ModeDir | 0755, -1, c4::ID());
    built.AddEntry(dir);
    auto child = makeEntry("f", 0644, 0, id);
    child.depth = 1;
    built.AddEntry(child);
    built.Canonicalize();
    REQUIRE(built.GetEntry("d/")->SizeIsNull());
}
