// SPDX-License-Identifier: Apache-2.0
// Base58 encoding/decoding for C4 IDs
//
// C4 uses a base58 alphabet (same as Bitcoin) to encode SHA-512 digests
// into 88-character strings, prefixed with "c4" for a total of 90 characters.
//
// Unlike Bitcoin's base58check, C4 uses fixed-width encoding: always 88
// base58 chars for 64 bytes. No leading-zero/leading-'1' convention.

#include "c4/c4.hpp"
#include "c4/c4.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Bitcoin base58 alphabet
constexpr char b58Alphabet[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Reverse lookup table: ASCII value -> base58 digit (255 = invalid)
constexpr std::array<uint8_t, 128> b58Reverse = []() {
    std::array<uint8_t, 128> table{};
    for (auto &v : table) v = 255;
    for (int i = 0; i < 58; i++) {
        table[static_cast<unsigned char>(b58Alphabet[i])] = static_cast<uint8_t>(i);
    }
    return table;
}();

// Encode raw bytes to base58 string (pure big-integer conversion, no
// leading-zero convention). Caller pads to desired width.
std::string base58Encode(const uint8_t *data, size_t len) {
    std::vector<uint8_t> digits;
    digits.reserve(len * 138 / 100 + 1); // log(256) / log(58)

    for (size_t i = 0; i < len; i++) {
        uint32_t carry = data[i];
        for (auto &d : digits) {
            carry += static_cast<uint32_t>(d) << 8;
            d = static_cast<uint8_t>(carry % 58);
            carry /= 58;
        }
        while (carry > 0) {
            digits.push_back(static_cast<uint8_t>(carry % 58));
            carry /= 58;
        }
    }

    std::string result;
    result.reserve(digits.size());
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        result += b58Alphabet[*it];
    }
    return result;
}

// Decode base58 string to raw bytes (pure big-integer conversion, no
// leading-'1' convention). Output is zero-padded to outlen.
bool base58Decode(const char *str, size_t slen, uint8_t *out, size_t outlen) {
    std::vector<uint8_t> bytes;
    bytes.reserve(slen * 733 / 1000 + 1);

    for (size_t i = 0; i < slen; i++) {
        auto c = static_cast<unsigned char>(str[i]);
        if (c >= 128 || b58Reverse[c] == 255) {
            return false;
        }
        uint32_t carry = b58Reverse[c];
        for (auto &b : bytes) {
            carry += static_cast<uint32_t>(b) * 58;
            b = static_cast<uint8_t>(carry & 0xff);
            carry >>= 8;
        }
        while (carry > 0) {
            bytes.push_back(static_cast<uint8_t>(carry & 0xff));
            carry >>= 8;
        }
    }

    if (bytes.size() > outlen) {
        return false;
    }

    // Reverse into output, zero-pad front
    size_t pad = outlen - bytes.size();
    std::memset(out, 0, pad);
    for (size_t i = 0; i < bytes.size(); i++) {
        out[pad + i] = bytes[bytes.size() - 1 - i];
    }
    return true;
}

} // anonymous namespace

namespace c4 {

std::string ID::String() const {
    std::string encoded = base58Encode(digest_.data(), digest_.size());
    // Pad to exactly 88 characters with '1' (base58 zero) — matches Go behavior
    if (encoded.size() < 88) {
        encoded.insert(0, 88 - encoded.size(), '1');
    }
    return "c4" + encoded;
}

ID ID::Parse(std::string_view str) {
    if (str.size() != IDLen) {
        throw std::invalid_argument("invalid C4 ID length");
    }
    if (str[0] != 'c' || str[1] != '4') {
        throw std::invalid_argument("C4 ID must start with 'c4'");
    }
    ID id;
    auto payload = str.substr(2);
    if (!base58Decode(payload.data(), payload.size(),
                      id.digest_.data(), id.digest_.size())) {
        throw std::invalid_argument("invalid base58 in C4 ID");
    }
    return id;
}

} // namespace c4

// C API implementation
#include "internal.h"

extern "C" {

c4_id_t *c4_id_new(void) {
    auto *id = new (std::nothrow) c4_id_t;
    if (id) {
        id->digest.fill(0);
    }
    return id;
}

void c4_id_free(c4_id_t *id) {
    delete id;
}

c4_error_t c4_identify(const void *data, size_t len, c4_id_t *out) {
    if (!out) return C4_ERR_INVALID_INPUT;
    try {
        auto id = c4::ID::Identify(data, len);
        std::memcpy(out->digest.data(), id.Digest().data(), c4::DigestLen);
        return C4_OK;
    } catch (...) {
        return C4_ERR_IO;
    }
}

c4_error_t c4_id_string(const c4_id_t *id, char *buf, size_t buflen) {
    if (!id || !buf || buflen < c4::IDLen + 1) return C4_ERR_INVALID_INPUT;
    try {
        auto cpp_id = c4::ID::FromDigest(id->digest.data(), id->digest.size());
        std::string encoded = cpp_id.String();
        if (encoded.size() != c4::IDLen) return C4_ERR_INVALID_ID;
        std::memcpy(buf, encoded.c_str(), c4::IDLen);
        buf[c4::IDLen] = '\0';
        return C4_OK;
    } catch (...) {
        return C4_ERR_IO;
    }
}

c4_error_t c4_id_parse(const char *str, size_t len, c4_id_t *out) {
    if (!str || !out) return C4_ERR_INVALID_INPUT;
    try {
        auto id = c4::ID::Parse(std::string_view(str, len));
        std::memcpy(out->digest.data(), id.Digest().data(), c4::DigestLen);
        return C4_OK;
    } catch (...) {
        return C4_ERR_INVALID_ID;
    }
}

int c4_id_compare(const c4_id_t *a, const c4_id_t *b) {
    if (!a || !b) return 0;
    return std::memcmp(a->digest.data(), b->digest.data(), c4::DigestLen);
}

int c4_id_equal(const c4_id_t *a, const c4_id_t *b) {
    return c4_id_compare(a, b) == 0;
}

const uint8_t *c4_id_digest(const c4_id_t *id) {
    if (!id) return nullptr;
    return id->digest.data();
}

c4_error_t c4_id_from_digest(const void *digest, size_t len, c4_id_t *out) {
    if (!digest || !out || len != c4::DigestLen) return C4_ERR_INVALID_INPUT;
    std::memcpy(out->digest.data(), digest, c4::DigestLen);
    return C4_OK;
}

} // extern "C"
