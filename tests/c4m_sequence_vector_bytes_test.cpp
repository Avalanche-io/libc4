// SPDX-License-Identifier: Apache-2.0
// Every c4m file in the three sequence vectors round-trips as its vector
// README's "## Round trip" table says (2026-11-16, for suite 1.0.27). A c4m
// file's byte ID is a C4 ID, so a round trip that changes a byte writes a
// different file. This is main's c4m_vector_bytes_test (c4 #158), scoped on
// the 1.0.27 line to the vectors whose tables this line carries:
// range-data-vector, sequence-null-size-vector and sequence-size-vector,
// copied from c4 master b82ec07. The other vectors vendored on this line
// carry 1.0.x READMEs with no table, because #158 is not on core's 1.0.27.
//
// Each file is decoded and encoded canonically. The table's second cell says
// what must come out:
//   itself        the output is the file, byte for byte;
//   `partner`     the output is that file, byte for byte;
//   ID in `f`     a chain: the resolved state's C4 ID is the one in f;
//   refused       decoding fails.
// Bytes are compared by SHA-512 from OpenSSL directly, never libc4's own C4
// ID code. Every vendored .c4m must be named by exactly one row.
//
// Three rows state 1.1 rules this line does not have; kLine replaces them
// with what the Go library on core's release/1.0.27 writes, as
// testdata/line-1.0.27/README.md records. The vector files themselves are
// master's, unedited.
//
// On this line Manifest::Parse does not resolve a chain (it skips bare C4
// IDs; resolving in the decoder is 1.1), so a file DecodePatchChain reads as
// more than one section is resolved through ResolvePatchChain.

#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <openssl/evp.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef C4M_TESTDATA_ROOT
#error "C4M_TESTDATA_ROOT must be defined by the build"
#endif

namespace {

namespace fs = std::filesystem;

const std::vector<std::string> kVectors = {
    "range-data-vector",
    "sequence-null-size-vector",
    "sequence-size-vector",
};

// The rows that state 1.1 rules, with what this line writes instead.
const std::map<std::string, std::string> kLine = {
    // 1.0.x drops an external base line on re-encode (keeping it is c4 #145).
    {"range-data-vector/add.c4m", "`../line-1.0.27/range-data-vector/add.c4m`"},
    {"range-data-vector/remove.c4m", "`../line-1.0.27/range-data-vector/remove.c4m`"},
    // On 1.0.x a 0 beside a non-empty C4 ID is a known 0 (null is c4 #68).
    {"sequence-null-size-vector/frames-zero-spelled.c4m", "itself"},
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

struct Row {
    std::string file;   // a name, relative to the vector directory
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

std::vector<std::string> c4mFilesIn(const fs::path &dir) {
    std::vector<std::string> out;
    for (const auto &e : fs::directory_iterator(dir)) {
        if (e.is_regular_file() && e.path().extension() == ".c4m")
            out.push_back(e.path().filename().generic_string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// decode reads text as this line's readers do: a chain through
// DecodePatchChain and ResolvePatchChain, anything else through Parse.
c4m::Manifest decode(const std::string &text) {
    auto sections = c4m::DecodePatchChain(std::string_view(text));
    if (sections.size() > 1)
        return c4m::ResolvePatchChain(sections);
    return c4m::Manifest::Parse(std::string_view(text));
}

void checkFile(const fs::path &dir, const std::string &name, const std::string &target) {
    const std::string in = readAll(dir / name);
    if (target == "refused") {
        CHECK_THROWS(decode(in));
        return;
    }
    c4m::Manifest m;
    try {
        m = decode(in);
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

TEST_CASE("sequence vectors: every c4m file round-trips as its README's table says, on this line",
          "[c4m][vector][bytes][sequence]") {
    const fs::path root(C4M_TESTDATA_ROOT);
    size_t checked = 0, overridden = 0;
    for (const auto &vname : kVectors) {
        INFO(vname);
        const fs::path dir = root / vname;
        const auto files = c4mFilesIn(dir);
        REQUIRE_FALSE(files.empty());
        const auto rows = roundTripRows(readAll(dir / "README.md"));
        REQUIRE_FALSE(rows.empty()); // a vendored vector states its round trip

        std::map<std::string, std::vector<std::string>> named; // file -> its targets
        for (const auto &row : rows) {
            const std::string f = backticked(row.file);
            INFO("row " << row.file);
            REQUIRE_FALSE(f.empty());
            named[f].push_back(row.target);
        }
        for (const auto &f : files) {
            INFO(f);
            const auto it = named.find(f);
            REQUIRE(it != named.end());      // every vendored file is named
            REQUIRE(it->second.size() == 1); // by exactly one row
            std::string target = it->second.front();
            const auto line = kLine.find(vname + "/" + f);
            if (line != kLine.end()) {
                target = line->second;
                overridden++;
            }
            checkFile(dir, f, target);
            checked++;
        }
    }
    CHECK(overridden == kLine.size()); // every override names a vendored file
    CHECK(checked == 16);
}
