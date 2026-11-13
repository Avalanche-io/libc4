// SPDX-License-Identifier: Apache-2.0
// A folded range's ID list (the frame IDs) survives decoding and canonical
// re-encoding. The maintainer, 2026-11-13: "it is an error to drop the frame
// IDs in any context." Through 1.0.27's first cut, Parse discarded the
// inline list, so a re-encoded c4m file was a different file with a
// different byte ID. Back-ported to 1.0.27 as a data-loss fix (decided
// 2026-11-13 by the release lead; reversible).
//
// testdata/range-data/range-data.c4m was written by Go's c4m library at
// core's release/1.0.27: four frames with every size known, folded by
// SequenceDetector (minimum length 3), beside one plain file, then
// Marshal. range-data-byte-id.txt is the C4 ID of those bytes. The bytes
// are compared by SHA-512 from OpenSSL, not libc4's own C4 ID code.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <openssl/evp.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef C4M_RANGE_DATA_DIR
#error "C4M_RANGE_DATA_DIR must be defined by the build"
#endif

namespace {

std::string readAll(const std::string &name) {
    std::ifstream f(std::filesystem::path(C4M_RANGE_DATA_DIR) / name, std::ios::binary);
    REQUIRE(f.is_open());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
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

} // namespace

TEST_CASE("range data: a folded range's ID list survives decode and canonical encode",
          "[c4m][sequence][range-data]") {
    const std::string in = readAll("range-data.c4m");
    std::string byteID = readAll("range-data-byte-id.txt");
    while (!byteID.empty() && (byteID.back() == '\n' || byteID.back() == '\r'))
        byteID.pop_back();
    // The fixture is the file its byte ID names.
    REQUIRE(c4::ID::Identify(std::string_view(in)).String() == byteID);

    auto m = c4m::Manifest::Parse(std::string_view(in));
    const std::string out = m.Encode();
    CHECK(sha512Hex(out) == sha512Hex(in));
    CHECK(c4::ID::Identify(std::string_view(out)).String() == byteID);
}
