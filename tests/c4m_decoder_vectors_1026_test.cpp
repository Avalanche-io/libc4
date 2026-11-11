// SPDX-License-Identifier: Apache-2.0
// Cross-implementation conformance for the 1.0.26 decoder rules. The files
// under testdata/link-target-vector/, backslash-name-vector/ and
// damaged-line-vector/ are byte-identical copies of the Go reference repo's
// c4m/testdata/ directories of the same names; each README states what every
// implementation MUST do.
//
// - After a link operator, a single field is the C4 ID field; two fields are
//   a target and the C4 ID field. The pretty form's padding before the C4 ID
//   field is not part of a target.
// - A backslash always starts a two-byte escape, so a name or link target
//   ending in a backslash reads back as itself.
// - A damaged field is an error naming the line, never a value: a size token
//   ends at a space; each mode position holds only its own letter or "-"; a
//   C4 ID field that does not parse is refused; so is an impossible date.
//
// Manifest::Parse and DecodePatchChain share one entry-line parser; both
// are checked.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#ifndef C4M_VECTOR_ROOT
#error "C4M_VECTOR_ROOT must be defined by the build"
#endif

namespace {

std::string readAll(const std::string &vector, const std::string &name) {
    std::ifstream f(std::filesystem::path(C4M_VECTOR_ROOT) / vector / name, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string readTrimmed(const std::string &vector, const std::string &name) {
    std::string s = readAll(vector, name);
    while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

// What a link line must read back as: target, link kind, whether it has
// its own C4 ID.
struct Link {
    std::string target;
    int hard_link;
    bool has_id;
    bool operator==(const Link &o) const {
        return target == o.target && hard_link == o.hard_link && has_id == o.has_id;
    }
};

std::map<std::string, Link> links(const std::vector<c4m::Entry> &entries) {
    std::map<std::string, Link> got;
    for (const auto &e : entries)
        got[e.name] = Link{e.target, e.hard_link, !e.id.IsNil()};
    return got;
}

std::map<std::string, Link> wantLinks() {
    const std::string t =
        "c43312d4NsKWQvybWGtgLk6iaWeuHUjz6PzjYgD6bNgqkqk4u6ECWjD2v88R1eRBPw3nNQnG6QMrmofSZcKfmMmJpy";
    return {
        {"c4-prefix-null", {"c4studio", 0, false}},
        {"epoch-link-id", {"real.txt", 0, true}},
        {"epoch-link-nil", {"missing.txt", 0, false}},
        {"hard-null-id", {"", -1, true}},
        {"hard-null-nil", {"", -1, false}},
        {"id-exr-null", {t + ".exr", 0, false}},
        {"id-exr-set", {t + ".exr", 0, true}},
        {"id-null", {t, 0, false}},
        {"id-set", {t, 0, true}},
        {"objects-null", {"../objects/" + t, 0, false}},
    };
}

std::map<std::string, std::string> targets(const std::vector<c4m::Entry> &entries) {
    std::map<std::string, std::string> got;
    for (const auto &e : entries)
        got[e.name] = e.target;
    return got;
}

// Names and targets as on disk (after UnsafeName).
const std::map<std::string, std::string> wantBackslash = {
    {"plain.txt", ""},
    {"trail\\", ""},
    {"end\\\\", ""},
    {"a\\ b", ""},
    {"link", "target\\"},
    {"dir\\x/", ""},
    {"in.txt", ""},
};

std::vector<c4m::Entry> patchSection(const std::string &vector) {
    auto sections = c4m::DecodePatchChain(std::string_view(readAll(vector, "patch.c4m")));
    REQUIRE(sections.size() == 1);
    // The empty tree's ID is the C4 ID of its empty canonical text.
    CHECK(sections[0].baseID == c4::ID::Identify(std::string_view("")));
    return sections[0].entries;
}

} // namespace

TEST_CASE("link-target vector: canonical and pretty read every link as written", "[c4m][vector]") {
    auto form = GENERATE(as<std::string>{}, "canonical.c4m");
    INFO(form);
    auto m = c4m::Manifest::Parse(readAll("link-target-vector", form));
    CHECK(links(m.Entries()) == wantLinks());
    CHECK(m.ComputeC4ID().String() == readTrimmed("link-target-vector", "root-id.txt"));
}

TEST_CASE("link-target vector: patch is one section over the empty tree", "[c4m][vector]") {
    CHECK(links(patchSection("link-target-vector")) == wantLinks());
}

TEST_CASE("backslash-name vector: every name and target reads back as itself", "[c4m][vector]") {
    auto form = GENERATE(as<std::string>{}, "canonical.c4m");
    INFO(form);
    auto m = c4m::Manifest::Parse(readAll("backslash-name-vector", form));
    std::map<std::string, std::string> got;
    for (const auto *e : m.AllEntries())
        got[e->name] = e->target;
    CHECK(got == wantBackslash);
    CHECK(m.ComputeC4ID().String() == readTrimmed("backslash-name-vector", "root-id.txt"));
}

TEST_CASE("backslash-name vector: patch is one section over the empty tree", "[c4m][vector]") {
    CHECK(targets(patchSection("backslash-name-vector")) == wantBackslash);
}

TEST_CASE("damaged-line vector: ok.c4m decodes", "[c4m][vector]") {
    auto text = readAll("damaged-line-vector", "ok.c4m");
    CHECK(c4m::Manifest::Parse(text).Entries().size() == 1);
    CHECK(c4m::DecodePatchChain(std::string_view(text)).size() == 1);
}

TEST_CASE("damaged-line vector: every damaged file is refused, naming line 1", "[c4m][vector]") {
    auto name = GENERATE(as<std::string>{}, "id-89.c4m", "id-91.c4m", "id-bad-char.c4m",
                         "size-junk.c4m", "size-null-junk.c4m", "mode-bad-char.c4m",
                         "mode-wrong-letter.c4m", "mode-sticky-letter-misplaced.c4m",
                         "time-impossible.c4m");
    INFO(name);
    auto text = readAll("damaged-line-vector", name);
    for (int reader = 0; reader < 2; reader++) {
        INFO((reader == 0 ? "Manifest::Parse" : "DecodePatchChain"));
        std::string err;
        try {
            if (reader == 0)
                c4m::Manifest::Parse(text);
            else
                c4m::DecodePatchChain(std::string_view(text));
        } catch (const std::runtime_error &e) {
            err = e.what();
        } catch (const std::exception &e) {
            err = std::string("unnamed: ") + e.what();
        }
        INFO("error: " << err);
        CHECK(err.find("line 1:") != std::string::npos);
    }
}

// This release line does not read the pretty form (main does). It must
// refuse a pretty file loudly, naming the first line it cannot read, never
// misread it. A pretty line with a null timestamp parses, so the error may
// name a later line than 1.
TEST_CASE("1.0.x: every pretty vector file is refused, naming a line", "[c4m][vector]") {
    auto path = GENERATE(as<std::string>{}, "link-target-vector/pretty.c4m",
                         "backslash-name-vector/pretty.c4m", "empty-tree-vector/empty-dir-pretty.c4m");
    INFO(path);
    auto slash = path.find('/');
    std::string err;
    try {
        c4m::Manifest::Parse(readAll(path.substr(0, slash), path.substr(slash + 1)));
    } catch (const std::runtime_error &e) {
        err = e.what();
    }
    CHECK(err.find("c4m: line ") != std::string::npos);
    CHECK(err.find("cannot parse timestamp") != std::string::npos);
}
