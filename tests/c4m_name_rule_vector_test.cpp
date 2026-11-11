// SPDX-License-Identifier: Apache-2.0
// Cross-implementation name-rule vector (c4 core, c4m/testdata/
// name-rule-vector; the files under testdata/name-rule-vector/ are
// byte-identical copies). A name is a bare entry name, never a path: only
// "/" separates paths in c4m, and a name may end in "/" only as a
// directory's marker; ".", "..", "./", "../" and "/" are path components.
// Through 1.0.26 libc4 cut a name at its first "/" and dropped the rest of
// the line, C4 ID included ("sub/real.txt" read as a directory "sub/"),
// read the path components as names, and wrote all of them raw.
#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>
#include <string>

#ifndef C4M_NAME_RULE_VECTOR_DIR
#error "C4M_NAME_RULE_VECTOR_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_NAME_RULE_VECTOR_DIR) / name;
}

const std::map<std::string, std::string> kCases = {
    {"file-sub", "sub/real.txt"},       {"file-leading-slash", "/real.txt"},
    {"file-trailing-path", "real.txt/x"}, {"file-dotdot-inside", "a/../real.txt"},
    {"file-dot", "."},                  {"file-dotdot", ".."},
    {"dir-nested", "a/empty/"},         {"dir-dot", "./"},
    {"dir-dotdot", "../"},              {"dir-root", "/"},
    {"dir-double-slash", "empty//"},
};

c4m::Entry entryNamed(const std::string &name) {
    c4m::Entry e;
    e.mode = name.back() == '/' ? (c4m::ModeDir | 0755u) : 0644u;
    e.timestamp = 1767323045;
    e.size = 1;
    e.name = name;
    return e;
}

} // namespace

TEST_CASE("name-rule vector: both ok lines decode", "[c4m][vector][name]") {
    for (const char *ok : {"ok-file.c4m", "ok-dir.c4m"}) {
        c4m::Manifest m = c4m::Manifest::ParseFile(fixture(ok));
        CHECK(m.EntryCount() == 1);
    }
}

TEST_CASE("name-rule vector: every damaged name is refused, naming line 1", "[c4m][vector][name]") {
    for (const auto &[file, name] : kCases) {
        INFO(file);
        try {
            (void)c4m::Manifest::ParseFile(fixture(file + ".c4m"));
            FAIL_CHECK(file << " decoded instead of being refused");
        } catch (const std::runtime_error &e) {
            CHECK(std::string(e.what()).find("line 1") != std::string::npos);
        }
    }
}

TEST_CASE("name-rule vector: a forbidden name is never written", "[c4m][vector][name]") {
    for (const auto &[file, name] : kCases) {
        INFO(name);
        c4m::Manifest m;
        m.AddEntry(entryNamed(name));
        CHECK_THROWS(m.Encode());
        c4m::PatchResult pr;
        pr.patch = m.Copy();
        CHECK_THROWS(c4m::EncodePatch(pr));
    }
    c4m::Manifest ok;
    ok.AddEntry(entryNamed("empty/"));
    CHECK(ok.Encode().find(" empty/ ") != std::string::npos);
}

TEST_CASE("name-rule vector: a forbidden name has no C4 ID, flat or nested", "[c4m][vector][name]") {
    c4m::Manifest flat;
    flat.AddEntry(entryNamed("sub/real.txt"));
    c4m::Manifest nested;
    nested.AddEntry(entryNamed("d/"));
    c4m::Entry deep = entryNamed("../real.txt");
    deep.depth = 1;
    nested.AddEntry(deep);
    CHECK(flat.ComputeC4ID().IsNil());
    CHECK(nested.ComputeC4ID().IsNil());
    c4m::Manifest ok;
    ok.AddEntry(entryNamed("real.txt"));
    CHECK_FALSE(ok.ComputeC4ID().IsNil());
}

TEST_CASE("name-rule vector: a backslash is a literal name character", "[c4m][vector][name]") {
    c4m::Manifest m;
    m.AddEntry(entryNamed("a\\b.txt"));
    CHECK_NOTHROW(m.Encode());
    CHECK_FALSE(m.ComputeC4ID().IsNil());
    // Through 1.0.26 Validate counted a backslash as a path separator.
    CHECK_NOTHROW(m.Validate());
}
