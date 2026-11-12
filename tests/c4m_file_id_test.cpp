// SPDX-License-Identifier: Apache-2.0
// A file's C4 ID summons exactly that file, on every path, and c4m text is
// no exception (the C4 rule of 2026-11-03; the Go reference's
// design/no-self-id.md). Through 1.0.26 libc4's IdentifyFile and
// c4_identify_file re-rendered a .c4m-named file that parsed as c4m before
// hashing it, and the c4m-aware calls did the same for anything that
// looked like c4m, so the ID named other bytes.
//
// The fixtures under testdata/file-id/ were written by the Go reference's
// CLI (release/1.0.27): `c4 id -e` (pretty, nested and flat), `c4 id`
// (canonical nested) and a `c4 diff` chain. The expected byte IDs below
// were computed independently of libc4 (Python hashlib SHA-512 and a
// separate base58 encoder), and are pinned here as data.

#include "c4/c4.hpp"
#include "c4/c4.h"
#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#ifndef C4M_FILE_ID_DIR
#error "C4M_FILE_ID_DIR must be defined by the build"
#endif

namespace {

const std::map<std::string, std::string> kByteID = {
    {"nested-e.c4m", "c45pU61ufXJGqD2UXGEv2wzxUFbkMqS1GRdzUZyfGS7qzHWxBK7qTALjqGX36ashw3eVhRBfyCynCVUc1kykTvJqju"},
    {"flat-e.c4m", "c43HYVn2hRcB3spsfSrDdEwExfNNhgdB5m8deiEexxCgg4xWKBSFDwSE6ZuHZjbxQgJUDcyBvHGkA1vw5sDUxtyu9B"},
    {"nested-id.c4m", "c42SNjfcoTUnRHiudjq38pacseuNigxp72BZMorkko4uWYq35iVhhoM5T44uUNrBtF9nQhx93SyiheFSASteNu738r"},
    {"diff-chain.c4m", "c45tRm74LqRzaqPBjgonfX5uFYCcxsYaD4JjMVR2VkCwYPFnrkKLTUDLc6N8uoGNUrCU8yHp7qXFCvtbYbJmEJ8YoB"},
};

std::filesystem::path fixture(const std::string &name) {
    return std::filesystem::path(C4M_FILE_ID_DIR) / name;
}

std::string readAll(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string cString(const c4_id_t *id) {
    char buf[C4_ID_LEN + 1];
    REQUIRE(c4_id_string(id, buf, sizeof buf) == C4_OK);
    return std::string(buf);
}

} // namespace

TEST_CASE("file ID: every identify path gives a c4m file its byte ID", "[c4m][file-id]") {
  for (const auto &[source, want] : kByteID) {
    INFO(source);
    const std::string bytes = readAll(fixture(source));

    // Bytes, string_view and stream.
    CHECK(c4::ID::Identify(std::string_view(bytes)).String() == want);
    CHECK(c4::ID::Identify(bytes.data(), bytes.size()).String() == want);
    std::istringstream in(bytes);
    CHECK(c4::ID::Identify(in).String() == want);

    // A file by path, under a .c4m name and under others: the name never
    // changes what a file's ID is.
    auto dir = std::filesystem::temp_directory_path() / ("c4-file-id-" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    for (const std::string name : {"tree.c4m", "tree.bin", "tree.txt"}) {
        INFO(name);
        auto p = dir / name;
        {
            std::ofstream out(p, std::ios::binary);
            out << bytes;
        }
        CHECK(c4::ID::IdentifyFile(p).String() == want);

        c4_id_t *id = c4_id_new();
        REQUIRE(c4_identify_file(p.string().c_str(), id) == C4_OK);
        CHECK(cString(id) == want);

        int fd = ::open(p.string().c_str(), O_RDONLY);
        REQUIRE(fd >= 0);
        REQUIRE(c4_identify_fd(fd, id) == C4_OK);
        ::close(fd);
        CHECK(cString(id) == want);
        c4_id_free(id);
    }
    std::filesystem::remove_all(dir);

    c4_id_t *id = c4_id_new();
    REQUIRE(c4_identify(bytes.data(), bytes.size(), id) == C4_OK);
    CHECK(cString(id) == want);
    c4_id_free(id);
  }
}

TEST_CASE("file ID: a parseable non-canonical c4m's byte ID is not its description ID", "[c4m][file-id]") {
    // The c4m diff chain parses on every line, so this is the case where a
    // re-render would change the ID; the description ID is still there.
    const std::string bytes = readAll(fixture("diff-chain.c4m"));
    auto m = c4m::Manifest::Parse(bytes);
    CHECK(m.ComputeC4ID().String() != kByteID.at("diff-chain.c4m"));
    CHECK(c4::ID::IdentifyFile(fixture("diff-chain.c4m")).String() == kByteID.at("diff-chain.c4m"));
}
