// SPDX-License-Identifier: Apache-2.0
#include "c4/c4.hpp"
#include "c4/c4.h"

#include <openssl/evp.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

// Thread-local EVP_MD_CTX reuse. Allocated once per thread, never freed
// during the thread's lifetime. Avoids repeated malloc/free in hot paths
// (Sum, Identify). Reset between uses via EVP_DigestInit_ex.
EVP_MD_CTX *getThreadCtx() {
    thread_local EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    return ctx;
}

} // anonymous namespace

namespace c4 {

ID::ID() {
    digest_.fill(0);
}

ID ID::Identify(const void *data, size_t len) {
    ID id;
    EVP_MD_CTX *ctx = getThreadCtx();
    if (!ctx) {
        throw std::runtime_error("failed to create digest context");
    }
    unsigned int digest_len = 0;
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, id.digest_.data(), &digest_len) != 1) {
        throw std::runtime_error("SHA-512 digest failed");
    }
    return id;
}

ID ID::Identify(std::string_view data) {
    return Identify(data.data(), data.size());
}

ID ID::Identify(std::istream &stream) {
    EVP_MD_CTX *ctx = getThreadCtx();
    if (!ctx) {
        throw std::runtime_error("failed to create digest context");
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) != 1) {
        throw std::runtime_error("SHA-512 init failed");
    }

    // 256KB read buffer for better I/O throughput on large files
    char buf[262144];
    while (stream.read(buf, sizeof(buf)) || stream.gcount() > 0) {
        if (EVP_DigestUpdate(ctx, buf, static_cast<size_t>(stream.gcount())) != 1) {
            throw std::runtime_error("SHA-512 update failed");
        }
    }

    ID id;
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, id.digest_.data(), &digest_len) != 1) {
        throw std::runtime_error("SHA-512 finalize failed");
    }
    return id;
}

ID ID::IdentifyFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    return Identify(file);
}

ID ID::FromDigest(const uint8_t *data, size_t len) {
    if (len != DigestLen) {
        throw std::invalid_argument("digest must be exactly 64 bytes");
    }
    ID id;
    std::memcpy(id.digest_.data(), data, DigestLen);
    return id;
}

ID ID::Sum(const ID &other) const {
    int cmp = std::memcmp(digest_.data(), other.digest_.data(), DigestLen);
    if (cmp == 0) {
        return *this;
    }

    const ID &smaller = (cmp < 0) ? *this : other;
    const ID &larger  = (cmp < 0) ? other : *this;

    ID result;
    EVP_MD_CTX *ctx = getThreadCtx();
    if (!ctx) {
        throw std::runtime_error("failed to create digest context");
    }
    unsigned int digest_len = 0;
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, smaller.digest_.data(), DigestLen) != 1 ||
        EVP_DigestUpdate(ctx, larger.digest_.data(), DigestLen) != 1 ||
        EVP_DigestFinal_ex(ctx, result.digest_.data(), &digest_len) != 1) {
        throw std::runtime_error("SHA-512 digest failed");
    }
    return result;
}

const std::array<uint8_t, DigestLen> &ID::Digest() const {
    return digest_;
}

bool ID::operator==(const ID &other) const {
    return digest_ == other.digest_;
}

bool ID::operator!=(const ID &other) const {
    return digest_ != other.digest_;
}

bool ID::operator<(const ID &other) const {
    return digest_ < other.digest_;
}

bool ID::IsNil() const {
    return std::all_of(digest_.begin(), digest_.end(),
                       [](uint8_t b) { return b == 0; });
}

std::ostream &operator<<(std::ostream &os, const ID &id) {
    return os << id.String();
}

} // namespace c4

// std::hash
size_t std::hash<c4::ID>::operator()(const c4::ID &id) const noexcept {
    // Use first 8 bytes of the digest as hash — plenty of entropy
    const auto &d = id.Digest();
    size_t h = 0;
    std::memcpy(&h, d.data(), sizeof(h));
    return h;
}
