// SPDX-License-Identifier: Apache-2.0
// Cross-implementation chain-vector conformance test (grammar erratum,
// 2026-07-13; amended 2026-11-08). The fixtures under testdata/chain-vector/
// are byte-identical copies of oss/c4/c4m/testdata/chain-vector/ -- the
// shared contract every c4m implementation must satisfy. See that
// directory's README.md.
//
//   vector.c4m         MUST resolve to exactly resolved-root-id.txt
//   bad-validator.c4m  MUST be rejected: its bare ID at EOF mismatches the
//                      accumulated state (the resolving parser verifies it
//                      first) and ends the file (the shape reader)
//   bad-checkpoint.c4m MUST be rejected (interior checkpoint mismatch)
//
// A stream ends on an entry line: a bare C4 ID at end of input is a format
// error (ruled 2026-11-08), TrailingIDError on every reader.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#ifndef C4M_CHAIN_VECTOR_DIR
#error "C4M_CHAIN_VECTOR_DIR must be defined by the build"
#endif

using Catch::Matchers::ContainsSubstring;

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_CHAIN_VECTOR_DIR) / name;
}

std::string readAll(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Read a file and strip trailing whitespace/newlines.
std::string readTrimmed(const std::filesystem::path &p) {
    std::string s = readAll(p);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::vector<std::string> lines(const std::string &text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
        out.push_back(line);
    return out;
}

// Every reader entry point, by name, over the same text. ParseFile reads a
// copy written to a scratch file.
struct Reader {
    std::string name;
    std::function<void(const std::string &)> read;
};

std::vector<Reader> readers() {
    return {
        {"Manifest::Parse(string_view)",
         [](const std::string &t) { c4m::Manifest::Parse(std::string_view(t)); }},
        {"Manifest::Parse(istream)",
         [](const std::string &t) {
             std::istringstream in(t);
             c4m::Manifest::Parse(in);
         }},
        {"Manifest::ParseFile",
         [](const std::string &t) {
             auto p = std::filesystem::temp_directory_path() / "libc4-trailing-id-test.c4m";
             {
                 std::ofstream f(p, std::ios::binary);
                 f << t;
             }
             try {
                 c4m::Manifest::ParseFile(p);
             } catch (...) {
                 std::filesystem::remove(p);
                 throw;
             }
             std::filesystem::remove(p);
         }},
        {"DecodePatchChain(string_view)",
         [](const std::string &t) { c4m::DecodePatchChain(std::string_view(t)); }},
        {"DecodePatchChain(istream)",
         [](const std::string &t) {
             std::istringstream in(t);
             c4m::DecodePatchChain(in);
         }},
    };
}

// The shape `c4 diff >>` wrote through c4 1.0.26: the chain, then the
// resolved state's own ID on a bare final line.
std::string vectorWithTrailingID() {
    return readAll(fixture("vector.c4m")) + readTrimmed(fixture("resolved-root-id.txt")) + "\n";
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
    REQUIRE(m.ComputeC4ID() == expected);

    // The stream ends on its last entry line, and every reader accepts it.
    for (const auto &r : readers()) {
        INFO(r.name);
        CHECK_NOTHROW(r.read(readAll(fixture("vector.c4m"))));
    }
    auto sections = c4m::DecodePatchChain(std::string_view(readAll(fixture("vector.c4m"))));
    REQUIRE(sections.size() == 2);
    CHECK(c4m::ResolvePatchChain(sections).ComputeC4ID() == expected);
}

TEST_CASE("chain-vector: bad-validator.c4m is rejected", "[c4m][chain][vector]") {
    const std::string text = readAll(fixture("bad-validator.c4m"));

    // The resolving parser verifies the bare ID as a checkpoint before it
    // reaches end of input, so the wrong ID is a mismatch, not a trailing ID
    // (the Go reference pins ErrPatchIDMismatch here).
    for (const auto &r : readers()) {
        if (r.name.rfind("Manifest::", 0) != 0)
            continue;
        INFO(r.name);
        try {
            r.read(text);
            FAIL("accepted");
        } catch (const c4m::TrailingIDError &) {
            FAIL("TrailingIDError, want a checkpoint mismatch");
        } catch (const std::runtime_error &e) {
            CHECK_THAT(e.what(), ContainsSubstring("patch ID mismatch (line 4)"));
        }
    }

    // The shape reader does not verify, so the trailing ID is what it refuses.
    for (const auto &r : readers()) {
        if (r.name.rfind("DecodePatchChain", 0) != 0)
            continue;
        INFO(r.name);
        CHECK_THROWS_AS(r.read(text), c4m::TrailingIDError);
    }
}

TEST_CASE("chain-vector: bad-checkpoint.c4m is rejected", "[c4m][chain][vector]") {
    // The interior checkpoint does not match the accumulated base state.
    REQUIRE_THROWS_WITH(c4m::Manifest::ParseFile(fixture("bad-checkpoint.c4m")),
                        ContainsSubstring("patch ID mismatch (line 2)"));
}

TEST_CASE("chain-vector: a correct trailing ID is refused on every reader", "[c4m][chain][vector]") {
    // Correct, so no reader can mistake it for a checkpoint mismatch: the
    // refusal is the format rule alone.
    const std::string text = vectorWithTrailingID();
    for (const auto &r : readers()) {
        INFO(r.name);
        try {
            r.read(text);
            FAIL("accepted");
        } catch (const c4m::TrailingIDError &e) {
            CHECK(e.line() == 4);
            CHECK(std::string(e.what()) ==
                  "c4m: a bare C4 ID ends the file; a c4m file never ends with its own ID (line 4): "
                  "delete that line, or rewrite the file with `c4 id FILE.c4m > fixed.c4m`");
        }
    }

    // Blank lines after the ID do not hide it.
    for (const auto &r : readers()) {
        INFO(r.name);
        CHECK_THROWS_AS(r.read(text + "\n\n"), c4m::TrailingIDError);
    }
}

TEST_CASE("chain-vector: consecutive checkpoints followed by a patch are accepted", "[c4m][chain][vector]") {
    // vector.c4m with its checkpoint repeated: both name the same state.
    auto ls = lines(readAll(fixture("vector.c4m")));
    REQUIRE(ls.size() == 3);
    const std::string text = ls[0] + "\n" + ls[1] + "\n" + ls[1] + "\n" + ls[2] + "\n";

    for (const auto &r : readers()) {
        INFO(r.name);
        CHECK_NOTHROW(r.read(text));
    }
    auto m = c4m::Manifest::Parse(std::string_view(text));
    CHECK(m.ComputeC4ID() == c4::ID::Parse(readTrimmed(fixture("resolved-root-id.txt"))));
    auto sections = c4m::DecodePatchChain(std::string_view(text));
    REQUIRE(sections.size() == 2);
    CHECK(sections[1].baseID.String() == ls[1]);

    // Repeated at end of input with no patch after them, they are still a
    // trailing ID.
    const std::string trailing = ls[0] + "\n" + ls[1] + "\n" + ls[1] + "\n";
    for (const auto &r : readers()) {
        INFO(r.name);
        CHECK_THROWS_AS(r.read(trailing), c4m::TrailingIDError);
    }
}

TEST_CASE("chain-vector: a file that is only a base reference is not a trailing ID", "[c4m][chain][vector]") {
    // One bare ID on the first line is an external base reference with no
    // entries: the resolving parser keeps it as the base, the shape reader
    // returns no sections (unchanged by the 2026-11-08 rule).
    const std::string id = readTrimmed(fixture("resolved-root-id.txt"));
    const std::string text = id + "\n";

    c4m::Manifest m;
    REQUIRE_NOTHROW(m = c4m::Manifest::Parse(std::string_view(text)));
    CHECK(m.Base().String() == id);
    CHECK(m.EntryCount() == 0);

    std::vector<c4m::PatchSection> sections;
    REQUIRE_NOTHROW(sections = c4m::DecodePatchChain(std::string_view(text)));
    CHECK(sections.empty());
}
