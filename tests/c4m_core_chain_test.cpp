// SPDX-License-Identifier: Apache-2.0
// A changeset libc4 writes, appended to a c4m core wrote, reads back in
// libc4 and in core (2026-11-16, for suite 1.0.27).
//
// testdata/core-1027-chain/ is written by core release/1.0.27 (eb33848):
// base.c4m by `c4 id` on a tree, v2.c4m by `c4 id` on the same tree with a
// file changed, one removed and one added, changeset.c4m by
// `c4 diff base.c4m v2.c4m`, and chain.c4m is base.c4m then changeset.c4m,
// byte for byte (`c4 diff >>`). Core resolves chain.c4m to v2.c4m byte for
// byte, through its library and through `c4 patch`.
//
// A c4m file never ends with a bare C4 ID: ruled 2026-11-08 by the
// maintainer, "A c4 id at the end of a file is no different than a generic
// data file that happens to contain c4 ids... If we are talking about a c4id
// at the end of a c4m file, then this is an error in format." Core's 1.0.27
// c4 diff dropped the trailing new-state ID (3ecd6ea). Through 1.0.26
// libc4's EncodePatch still wrote it.
//
// When the build defines C4M_CORE_BIN (cmake -DC4_CORE_BIN=<path to core's
// c4 at release/1.0.27>; CI builds it), the appended file is also resolved
// by that binary's `c4 patch`.

#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_CORE_CHAIN_DIR
#error "C4M_CORE_CHAIN_DIR must be defined by the build"
#endif

namespace {

namespace fs = std::filesystem;

std::string readAll(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string fixture(const std::string &name) {
    return readAll(fs::path(C4M_CORE_CHAIN_DIR) / name);
}

void writeAll(const fs::path &p, const std::string &s) {
    std::ofstream f(p, std::ios::binary);
    REQUIRE(f.is_open());
    f << s;
}

std::string lastLine(const std::string &s) {
    std::string t = s;
    while (!t.empty() && t.back() == '\n')
        t.pop_back();
    auto nl = t.rfind('\n');
    return nl == std::string::npos ? t : t.substr(nl + 1);
}

bool isBareID(const std::string &line) {
    return line.size() == 90 && line[0] == 'c' && line[1] == '4';
}

// libc4's reader for a chain on this line.
std::string resolveInLibc4(const std::string &text) {
    return c4m::ResolvePatchChain(c4m::DecodePatchChain(std::string_view(text))).Encode();
}

} // namespace

TEST_CASE("core chain: core's own chain resolves in libc4 to core's v2.c4m",
          "[c4m][chain][core-1027]") {
    CHECK(resolveInLibc4(fixture("chain.c4m")) == fixture("v2.c4m"));
}

TEST_CASE("core chain: EncodePatch writes what core's c4 diff writes, and no bare ID ends it",
          "[c4m][chain][core-1027]") {
    auto pr = c4m::PatchDiff(c4m::Manifest::Parse(std::string_view(fixture("base.c4m"))),
                             c4m::Manifest::Parse(std::string_view(fixture("v2.c4m"))));
    const std::string cs = c4m::EncodePatch(pr);
    CHECK_FALSE(isBareID(lastLine(cs)));
    CHECK(cs == fixture("changeset.c4m"));
}

TEST_CASE("core chain: a libc4 changeset appended to core's base.c4m reads back in libc4 and core",
          "[c4m][chain][core-1027]") {
    auto pr = c4m::PatchDiff(c4m::Manifest::Parse(std::string_view(fixture("base.c4m"))),
                             c4m::Manifest::Parse(std::string_view(fixture("v2.c4m"))));
    const std::string appended = fixture("base.c4m") + c4m::EncodePatch(pr);
    CHECK(resolveInLibc4(appended) == fixture("v2.c4m"));

#ifdef C4M_CORE_BIN
    // Core's c4 patch, isolated from the real store and home.
    const fs::path dir = fs::temp_directory_path() / ("libc4-core-chain-" + std::to_string(std::rand()));
    fs::create_directories(dir / "home");
    writeAll(dir / "appended.c4m", appended);
    const std::string cmd = "HOME='" + (dir / "home").string() + "' USERPROFILE='" + (dir / "home").string() +
                            "' C4_STORE='" + (dir / "home" / "store").string() + "' '" C4M_CORE_BIN "' patch '" +
                            (dir / "appended.c4m").string() + "' > '" + (dir / "out.c4m").string() + "' 2> '" +
                            (dir / "err.txt").string() + "'";
    const int rc = std::system(cmd.c_str());
    INFO("core stderr: " << readAll(dir / "err.txt"));
    CHECK(rc == 0);
    CHECK(readAll(dir / "out.c4m") == fixture("v2.c4m"));
    fs::remove_all(dir);
#endif
}

// ---- Manifest::Parse resolves a chain, as core release/1.0.27's Decode does ----
//
// Through 1.0.26 Parse skipped every bare C4 ID and concatenated the
// sections, so a chain read as its base's entries followed by its patches'
// lines: removals stayed, modifications appeared twice, and no error was
// raised. On this line, as in core's 1.0.27 decoder: a first-line bare ID is
// the external base; each later one is a block link that ends a section,
// and the section after it is applied as a patch; a boundary with no
// entries after it (an empty patch, including a bare ID at the end of the
// input) is refused.

TEST_CASE("core chain: Parse resolves core's chain to core's v2.c4m",
          "[c4m][chain][core-1027][parse]") {
    auto m = c4m::Manifest::Parse(std::string_view(fixture("chain.c4m")));
    CHECK(m.GetEntry("sub/gone.txt") == nullptr);
    CHECK(m.Encode() == fixture("v2.c4m"));
}

TEST_CASE("core chain: Parse resolves a libc4 changeset appended to core's base.c4m",
          "[c4m][chain][core-1027][parse]") {
    auto pr = c4m::PatchDiff(c4m::Manifest::Parse(std::string_view(fixture("base.c4m"))),
                             c4m::Manifest::Parse(std::string_view(fixture("v2.c4m"))));
    const std::string appended = fixture("base.c4m") + c4m::EncodePatch(pr);
    CHECK(c4m::Manifest::Parse(std::string_view(appended)).Encode() == fixture("v2.c4m"));
}

TEST_CASE("core chain: Parse refuses an empty patch section, as core's 1.0.27 decoder does",
          "[c4m][chain][core-1027][parse]") {
    const std::string base = fixture("base.c4m");
    const std::string id = c4m::Manifest::Parse(std::string_view(base)).ComputeC4ID().String();
    // A bare C4 ID at the end of the input.
    CHECK_THROWS(c4m::Manifest::Parse(std::string_view(base + id + "\n")));
    // Two boundaries with nothing between them, after a patch.
    const std::string chain = fixture("chain.c4m");
    const std::string v2id = c4m::Manifest::Parse(std::string_view(fixture("v2.c4m"))).ComputeC4ID().String();
    CHECK_THROWS(c4m::Manifest::Parse(std::string_view(chain + v2id + "\n" + v2id + "\n" + "-rw-r--r-- 2026-01-02T09:04:05Z 3 top.txt -\n")));
    // A first-line base followed by entries is not a chain, and still reads.
    CHECK_NOTHROW(c4m::Manifest::Parse(std::string_view(id + "\n" + base)));
}
