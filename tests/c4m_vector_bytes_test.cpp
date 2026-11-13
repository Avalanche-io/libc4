// SPDX-License-Identifier: Apache-2.0
// Every c4m file in every shared vector this port vendors round-trips as its
// vector README's "## Round trip" table says (c4 #158 at 7cdc9f3; Go's
// TestVectorRoundTrip reads the same table). A c4m file's byte ID is a C4
// ID, so a round trip that changes a byte writes a different file: until
// 2026-11-13 Parse dropped inline range data and nothing compared bytes, so
// the loss went unseen (decided 2026-11-13 by the release lead; reversible).
//
// Each file is decoded and encoded canonically. The table's second cell says
// what must come out:
//   itself        the output is the file, byte for byte;
//   `partner`     the output is that file, byte for byte;
//   ID in `f`     a chain: the resolved state's C4 ID is the one in f;
//   refused       decoding fails.
// Any other cell fails. Bytes are compared by SHA-512 from OpenSSL
// directly, never libc4's own C4 ID code. Every vendored .c4m must be named
// by exactly one row (a glob such as `*/*.c4m` counts); a row naming a file
// this port does not vendor is reported and passed over. The classification
// lives in the tables, the one statement every implementation shares, never
// in this file.

#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <openssl/evp.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef C4M_TESTDATA_ROOT
#error "C4M_TESTDATA_ROOT must be defined by the build"
#endif

namespace {

namespace fs = std::filesystem;

// Directories under testdata/ that are libc4's own fixtures, not vendored
// shared vectors, so they carry no README and no table.
const std::map<std::string, std::string> kOwnFixtures = {
    {"file-id", "Go CLI output pinned for its byte IDs (c4m_file_id_test.cpp)"},
};

std::string readAll(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string trim(std::string s) {
    const char *ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

std::string sha512Hex(const std::string &s) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int n = 0;
    REQUIRE(EVP_Digest(s.data(), s.size(), md, &n, EVP_sha512(), nullptr) == 1);
    static const char *hex = "0123456789abcdef";
    std::string out;
    for (unsigned int i = 0; i < n; i++) {
        out += hex[md[i] >> 4];
        out += hex[md[i] & 0xf];
    }
    return out;
}

// A cell's backticked name, or "" if the cell is not exactly one.
std::string backticked(const std::string &cell) {
    if (cell.size() < 3 || cell.front() != '`' || cell.back() != '`')
        return "";
    std::string inner = cell.substr(1, cell.size() - 2);
    return inner.find('`') == std::string::npos ? inner : "";
}

// '*' matches any run of characters but '/', '?' any one character but '/'.
bool globMatch(const char *pat, const char *s) {
    for (; *pat; ++pat, ++s) {
        if (*pat == '*') {
            for (const char *t = s;; ++t) {
                if (globMatch(pat + 1, t))
                    return true;
                if (*t == '\0' || *t == '/')
                    return false;
            }
        }
        if (*s == '\0' || *s == '/' ? *pat != *s : (*pat != '?' && *pat != *s))
            return false;
    }
    return *s == '\0';
}

struct Row {
    std::string file;   // a name or a glob, relative to the vector directory
    std::string target; // the second cell, trimmed
};

// The rows of the README's "## Round trip" table, after its header row.
std::vector<Row> roundTripRows(const std::string &readme) {
    std::vector<Row> rows;
    std::istringstream in(readme);
    std::string line;
    bool inSection = false, header = true;
    while (std::getline(in, line)) {
        if (line.rfind("## ", 0) == 0) {
            if (inSection)
                break;
            inSection = trim(line) == "## Round trip";
            continue;
        }
        if (!inSection || line.empty() || line[0] != '|')
            continue;
        std::vector<std::string> cells;
        std::stringstream ls(line.substr(1));
        std::string cell;
        while (std::getline(ls, cell, '|'))
            cells.push_back(trim(cell));
        if (cells.size() < 2)
            continue;
        if (cells[0].find_first_not_of("-: ") == std::string::npos)
            continue; // the separator row
        if (header) {
            header = false;
            continue;
        }
        rows.push_back({cells[0], cells[1]});
    }
    return rows;
}

std::vector<std::string> c4mFilesUnder(const fs::path &dir) {
    std::vector<std::string> out;
    for (const auto &e : fs::recursive_directory_iterator(dir)) {
        if (e.is_regular_file() && e.path().extension() == ".c4m")
            out.push_back(fs::relative(e.path(), dir).generic_string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

void checkFile(const fs::path &dir, const std::string &name, const std::string &target) {
    const std::string in = readAll(dir / name);
    if (target == "refused") {
        CHECK_THROWS(c4m::Manifest::Parse(std::string_view(in)));
        return;
    }
    c4m::Manifest m;
    try {
        m = c4m::Manifest::Parse(std::string_view(in));
    } catch (const std::exception &ex) {
        FAIL_CHECK("decode refused it: " << ex.what());
        return;
    }
    if (target == "itself") {
        CHECK(sha512Hex(m.Encode()) == sha512Hex(in));
        return;
    }
    if (target.rfind("ID in ", 0) == 0) {
        const std::string f = backticked(trim(target.substr(6)));
        REQUIRE_FALSE(f.empty());
        REQUIRE(fs::exists(dir / f));
        CHECK(m.ComputeC4ID().String() == trim(readAll(dir / f)));
        return;
    }
    const std::string partner = backticked(target);
    if (partner.empty()) {
        FAIL_CHECK("unknown round-trip entry: " << target);
        return;
    }
    INFO("partner " << partner);
    REQUIRE(fs::exists(dir / partner)); // a partner must be vendored with its file
    CHECK(sha512Hex(m.Encode()) == sha512Hex(readAll(dir / partner)));
}

} // namespace

TEST_CASE("shared vectors: every c4m file round-trips as its README's table says",
          "[c4m][vector][bytes]") {
    const fs::path root(C4M_TESTDATA_ROOT);
    std::vector<fs::path> dirs;
    for (const auto &e : fs::directory_iterator(root)) {
        if (e.is_directory())
            dirs.push_back(e.path());
    }
    std::sort(dirs.begin(), dirs.end());
    REQUIRE_FALSE(dirs.empty());

    size_t checked = 0;
    for (const auto &dir : dirs) {
        const std::string dname = dir.filename().generic_string();
        INFO(dname);
        const auto files = c4mFilesUnder(dir);
        if (!fs::exists(dir / "README.md")) {
            // Only a named fixture directory may go without a table.
            CHECK(kOwnFixtures.count(dname) == 1);
            continue;
        }
        const auto rows = roundTripRows(readAll(dir / "README.md"));
        if (files.empty())
            continue;
        REQUIRE_FALSE(rows.empty()); // a vendored vector states its round trip

        std::map<std::string, std::vector<std::string>> named; // file -> its targets
        for (const auto &row : rows) {
            const std::string pat = backticked(row.file);
            INFO("row " << row.file);
            REQUIRE_FALSE(pat.empty());
            bool any = false;
            for (const auto &f : files) {
                if (globMatch(pat.c_str(), f.c_str())) {
                    named[f].push_back(row.target);
                    any = true;
                }
            }
            if (!any)
                std::cout << "round trip: " << dname << "/" << pat
                          << " is in the table but not vendored here\n";
        }
        for (const auto &f : files) {
            INFO(f);
            const auto it = named.find(f);
            REQUIRE(it != named.end());          // every vendored file is named
            REQUIRE(it->second.size() == 1);     // by exactly one row
            checkFile(dir, f, it->second.front());
            checked++;
        }
    }
    CHECK(checked > 0);
}

TEST_CASE("shared vectors: the round-trip table parser", "[c4m][vector][bytes]") {
    const std::string readme =
        "# x\n\n## Round trip\n\nText.\n\n"
        "| File     | Re-encodes to |\n"
        "| -------- | ------------- |\n"
        "| `a.c4m`  | itself        |\n"
        "| `*/*.c4m`| refused       |\n\n"
        "## After\n\n| `z.c4m` | itself |\n";
    const auto rows = roundTripRows(readme);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].file == "`a.c4m`");
    CHECK(rows[0].target == "itself");
    CHECK(rows[1].target == "refused");
    CHECK(globMatch("*/*.c4m", "sub/a.c4m"));
    CHECK_FALSE(globMatch("*/*.c4m", "a.c4m"));
    CHECK_FALSE(globMatch("*.c4m", "sub/a.c4m"));
    CHECK(backticked("`x.c4m`") == "x.c4m");
    CHECK(backticked("ID in `x`").empty());
}
