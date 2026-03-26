// SPDX-License-Identifier: Apache-2.0
// C4 ID System - C++ API
// Reference implementation of SMPTE ST 2114:2017

#ifndef C4_C4_HPP
#define C4_C4_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace c4 {

constexpr size_t DigestLen = 64;  // SHA-512 digest
constexpr size_t IDLen = 90;      // Encoded string length

// A C4 ID: a 64-byte SHA-512 digest with base58 encoding.
class ID {
public:
    ID();

    // Identify data
    static ID Identify(const void *data, size_t len);
    static ID Identify(std::string_view data);
    static ID Identify(std::istream &stream);

    // Identify a file (c4m-aware: .c4m extension triggers canonical heuristic)
    static ID IdentifyFile(const std::filesystem::path &path);

    // c4m-aware identification: if data parses as a valid c4m file,
    // canonicalize it before hashing. Otherwise hash raw bytes.
    static ID IdentifyC4mAware(const void *data, size_t len);
    static ID IdentifyC4mAware(std::string_view data);

    // Parse from string
    static ID Parse(std::string_view str);

    // Construct from raw digest bytes (must be exactly DigestLen bytes)
    static ID FromDigest(const uint8_t *data, size_t len);

    // Encode to 90-character string
    std::string String() const;

    // Sum computes the combined ID of two IDs by hashing them in sorted
    // order (smaller digest first). If both IDs are equal, returns a copy.
    ID Sum(const ID &other) const;

    // Raw digest access
    const std::array<uint8_t, DigestLen> &Digest() const;

    // Comparison
    bool operator==(const ID &other) const;
    bool operator!=(const ID &other) const;
    bool operator<(const ID &other) const;

    // Check if this is a zero/nil ID
    bool IsNil() const;

private:
    std::array<uint8_t, DigestLen> digest_;
};

// Stream output
std::ostream &operator<<(std::ostream &os, const ID &id);

// A sorted set of IDs that can produce a tree ID.
class IDs {
public:
    IDs() = default;

    void Append(const class ID &id);
    void Append(class ID &&id);

    // Compute the C4 ID of this set (sorts, concatenates digests, hashes)
    class ID TreeID() const;

    size_t Size() const;
    bool Empty() const;

    const class ID &operator[](size_t i) const;

    // Iterators
    auto begin() const { return ids_.begin(); }
    auto end() const { return ids_.end(); }

private:
    std::vector<c4::ID> ids_;
};

} // namespace c4

// std::hash support for use in unordered containers
template <>
struct std::hash<c4::ID> {
    size_t operator()(const c4::ID &id) const noexcept;
};

#endif // C4_C4_HPP
