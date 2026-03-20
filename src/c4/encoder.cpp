// SPDX-License-Identifier: Apache-2.0
// Base58 encoding/decoding for C4 IDs
//
// C4 uses a base58 alphabet (same as Bitcoin) to encode SHA-512 digests
// into 88-character strings, prefixed with "c4" for a total of 90 characters.
//
// Unlike Bitcoin's base58check, C4 uses fixed-width encoding: always 88
// base58 chars for 64 bytes. No leading-zero/leading-'1' convention.
//
// This implementation uses 64-bit limb arithmetic for performance: instead
// of processing one base58 digit at a time, we pack up to 9 base58 digits
// per 64-bit limb (58^9 < 2^63), reducing the inner loop iterations ~9x.

#include "c4/c4.hpp"
#include "c4/c4.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

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

// Digits per limb for encoding (base58 -> bytes direction).
// 58^9 = 7,427,658,739,644,928. Max intermediate: (58^9-1)*256 + 255 < 2^64. Safe.
constexpr int kDigitsPerLimb = 9;
constexpr uint64_t POW58 = 7427658739644928ULL; // 58^9

// For a 64-byte (512-bit) input, we need at most 88 base58 digits.
// 88 / 9 = 10 limbs (ceil).
constexpr int kMaxLimbs = 10;

// Encode raw bytes to base58 string using 64-bit limb arithmetic.
// Each limb holds a value in [0, 58^9), representing 9 base58 digits.
std::string base58Encode(const uint8_t *data, size_t len) {
    uint64_t limbs[kMaxLimbs] = {};
    int nlimbs = 0;

    for (size_t i = 0; i < len; i++) {
        uint64_t carry = data[i];
        for (int j = 0; j < nlimbs; j++) {
            uint64_t acc = limbs[j] * 256 + carry;
            limbs[j] = acc % POW58;
            carry = acc / POW58;
        }
        while (carry > 0) {
            limbs[nlimbs++] = carry % POW58;
            carry /= POW58;
        }
    }

    // Extract base58 digits from limbs (most-significant limb first).
    // Each limb except the most-significant produces exactly kDigitsPerLimb digits.
    // The most-significant limb produces only as many digits as needed.
    char result[90];
    int pos = 0;

    if (nlimbs == 0) {
        return "";
    }

    // Most-significant limb: variable number of digits
    {
        char buf[kDigitsPerLimb];
        int n = 0;
        uint64_t v = limbs[nlimbs - 1];
        do {
            buf[n++] = b58Alphabet[v % 58];
            v /= 58;
        } while (v > 0);
        for (int k = n - 1; k >= 0; k--) {
            result[pos++] = buf[k];
        }
    }

    // Remaining limbs: each produces exactly kDigitsPerLimb digits
    for (int i = nlimbs - 2; i >= 0; i--) {
        char buf[kDigitsPerLimb];
        uint64_t v = limbs[i];
        for (int d = 0; d < kDigitsPerLimb; d++) {
            buf[d] = b58Alphabet[v % 58];
            v /= 58;
        }
        for (int k = kDigitsPerLimb - 1; k >= 0; k--) {
            result[pos++] = buf[k];
        }
    }

    return std::string(result, pos);
}

// Decode: limbs in base 256^8 = 18,446,744,073,709,551,616 would overflow,
// so we use 256^7 = 72,057,594,037,927,936. Max intermediate:
// (256^7 - 1) * 58 + 57 < 2^64. Safe.
constexpr int kBytesPerLimb = 7;
constexpr uint64_t POW256 = 72057594037927936ULL; // 256^7
constexpr int kDecodeMaxLimbs = 10; // ceil(64/7) = 10

bool base58Decode(const char *str, size_t slen, uint8_t *out, size_t outlen) {
    uint64_t limbs[kDecodeMaxLimbs] = {};
    int nlimbs = 0;

    for (size_t i = 0; i < slen; i++) {
        auto c = static_cast<unsigned char>(str[i]);
        if (c >= 128 || b58Reverse[c] == 255) {
            return false;
        }
        uint64_t carry = b58Reverse[c];
        for (int j = 0; j < nlimbs; j++) {
            uint64_t acc = limbs[j] * 58 + carry;
            limbs[j] = acc % POW256;
            carry = acc / POW256;
        }
        while (carry > 0) {
            if (nlimbs >= kDecodeMaxLimbs) {
                return false;
            }
            limbs[nlimbs++] = carry % POW256;
            carry /= POW256;
        }
    }

    // Extract bytes from limbs into a temp buffer.
    uint8_t temp[kDecodeMaxLimbs * kBytesPerLimb];
    int tpos = 0;

    if (nlimbs == 0) {
        std::memset(out, 0, outlen);
        return true;
    }

    // Most-significant limb: variable number of bytes
    {
        uint8_t buf[kBytesPerLimb];
        int n = 0;
        uint64_t v = limbs[nlimbs - 1];
        do {
            buf[n++] = static_cast<uint8_t>(v & 0xff);
            v >>= 8;
        } while (v > 0);
        for (int k = n - 1; k >= 0; k--) {
            temp[tpos++] = buf[k];
        }
    }

    // Remaining limbs: each produces exactly kBytesPerLimb bytes
    for (int i = nlimbs - 2; i >= 0; i--) {
        uint8_t buf[kBytesPerLimb];
        uint64_t v = limbs[i];
        for (int d = 0; d < kBytesPerLimb; d++) {
            buf[d] = static_cast<uint8_t>(v & 0xff);
            v >>= 8;
        }
        for (int k = kBytesPerLimb - 1; k >= 0; k--) {
            temp[tpos++] = buf[k];
        }
    }

    if (static_cast<size_t>(tpos) > outlen) {
        return false;
    }

    // Zero-pad front, copy into output
    size_t pad = outlen - static_cast<size_t>(tpos);
    std::memset(out, 0, pad);
    std::memcpy(out + pad, temp, tpos);
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
