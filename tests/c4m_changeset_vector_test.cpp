// SPDX-License-Identifier: Apache-2.0
// The shared changeset vector (c4 #175, c4m/testdata/changeset-vector),
// vendored byte for byte from c4 0b410a2. Its MUST rule: every patch
// writer, given base.c4m and v2.c4m, writes changeset.c4m byte for byte, and
// every chain reader resolves chain.c4m to v2.c4m. A changeset never ends
// with a bare C4 ID (ruled 2026-11-08), and its section is canonical: files
// before directories.

#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_TESTDATA_DIR
#error "C4M_TESTDATA_DIR must be defined by the build"
#endif

namespace {

std::string readAll(const std::string &name) {
    std::ifstream f(std::filesystem::path(C4M_TESTDATA_DIR) / "changeset-vector" / name, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

c4m::Manifest parse(const std::string &name) {
    return c4m::Manifest::Parse(std::string_view(readAll(name)));
}

} // namespace

TEST_CASE("changeset vector: EncodePatch(PatchDiff(base, v2)) is changeset.c4m byte for byte",
          "[c4m][vector][changeset]") {
    const std::string cs = c4m::EncodePatch(c4m::PatchDiff(parse("base.c4m"), parse("v2.c4m")));
    CHECK(cs == readAll("changeset.c4m"));
    std::string last = cs.substr(0, cs.size() - 1);
    last = last.substr(last.rfind('\n') + 1);
    CHECK_FALSE((last.size() == 90 && last.rfind("c4", 0) == 0));
}

TEST_CASE("changeset vector: chain.c4m resolves to v2.c4m through Parse and the chain readers",
          "[c4m][vector][changeset]") {
    auto m = parse("chain.c4m");
    CHECK(m.GetEntry("sub/gone.txt") == nullptr);
    CHECK(m.Encode() == readAll("v2.c4m"));
    auto r = c4m::ResolvePatchChain(c4m::DecodePatchChain(std::string_view(readAll("chain.c4m"))));
    CHECK(r.Encode() == readAll("v2.c4m"));
}

TEST_CASE("changeset vector: base.c4m with libc4's own changeset appended resolves to v2.c4m",
          "[c4m][vector][changeset]") {
    const std::string cs = c4m::EncodePatch(c4m::PatchDiff(parse("base.c4m"), parse("v2.c4m")));
    CHECK(c4m::Manifest::Parse(std::string_view(readAll("base.c4m") + cs)).Encode() == readAll("v2.c4m"));
}

TEST_CASE("changeset vector: changeset.c4m's patch applied to base.c4m is v2.c4m",
          "[c4m][vector][changeset]") {
    const std::string cs = readAll("changeset.c4m");
    auto patch = c4m::Manifest::Parse(std::string_view(cs.substr(cs.find('\n') + 1)));
    CHECK(c4m::ApplyPatch(parse("base.c4m"), patch).Encode() == readAll("v2.c4m"));
}
