// SPDX-License-Identifier: Apache-2.0
// C4M Format - C++ API
// Parser and encoder for .c4m capsule files.
// Reference: Go implementation at github.com/Avalanche-io/c4/c4m

#ifndef C4_C4M_HPP
#define C4_C4M_HPP

#include "c4.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace c4m {

// Mode type bits (matching Go's os.FileMode layout for cross-language compat)
constexpr uint32_t ModeDir        = 0x80000000;
constexpr uint32_t ModeSymlink    = 0x08000000;
constexpr uint32_t ModeDevice     = 0x04000000;
constexpr uint32_t ModeNamedPipe  = 0x02000000;
constexpr uint32_t ModeSocket     = 0x01000000;
constexpr uint32_t ModeSetuid     = 0x00800000;
constexpr uint32_t ModeSetgid     = 0x00400000;
constexpr uint32_t ModeSticky     = 0x00200000;
constexpr uint32_t ModeCharDevice = 0x00100000;
constexpr uint32_t ModeTypeMask   = ModeDir | ModeSymlink | ModeDevice |
                                     ModeNamedPipe | ModeSocket | ModeCharDevice;
constexpr uint32_t ModePermMask   = 0777;

// Null sentinel: timestamp 0 (Unix epoch) means "unspecified"
constexpr int64_t NullTimestamp = 0;
// Null sentinel: size -1 means "unspecified"
constexpr int64_t NullSize = -1;

// A single entry in a .c4m manifest.
struct Entry {
    uint32_t mode = 0;         // Unix file mode (type + permission bits)
    int64_t timestamp = 0;     // Modification time (Unix seconds UTC), 0 = null
    int64_t size = 0;          // File size in bytes, -1 = null
    std::string name;          // File or directory name (dirs end with '/')
    std::string target;        // Symlink target (empty if not a symlink)
    c4::ID id;                 // Content identifier
    int depth = 0;             // Nesting depth (0 = root level)

    bool IsDir() const;
    bool IsSymlink() const;

    // Format as canonical c4m line (no indentation, no trailing newline)
    std::string Canonical() const;

    // Format with indentation (no trailing newline)
    std::string Format(int indent_width = 2) const;
};

// A parsed .c4m manifest.
class Manifest {
public:
    Manifest() = default;

    // Parse from string
    static Manifest Parse(std::string_view data);

    // Parse from file
    static Manifest ParseFile(const std::filesystem::path &path);

    // Parse from stream
    static Manifest Parse(std::istream &stream);

    // Encode to canonical c4m format
    std::string Encode() const;

    // Write to file
    void WriteFile(const std::filesystem::path &path) const;

    // Sort entries (files before dirs at each level, natural sort within)
    void SortEntries();

    // Validate manifest structure (throws on error)
    void Validate() const;

    // Access fields
    const std::string &Version() const { return version_; }
    const c4::ID &Base() const { return base_; }
    const std::vector<Entry> &Entries() const { return entries_; }
    size_t EntryCount() const { return entries_.size(); }

    // Flat list of all entry pointers
    std::vector<const Entry *> AllEntries() const;

    // Lookup by name (linear scan)
    const Entry *GetEntry(const std::string &name) const;

    // Entries at a specific depth
    std::vector<const Entry *> GetEntriesAtDepth(int depth) const;

    // Computed C4 ID of the canonical form
    c4::ID RootID() const;

    // Mutators
    void AddEntry(Entry e);
    void SetVersion(const std::string &v) { version_ = v; }
    void SetBase(const c4::ID &id) { base_ = id; }

    // Scan directory (not yet implemented)
    static Manifest Scan(const std::filesystem::path &dir);

    // Set operations (not yet implemented)
    Manifest Diff(const Manifest &other) const;
    Manifest Merge(const Manifest &other) const;
    Manifest Intersect(const Manifest &other) const;

private:
    std::string version_ = "1.0";
    std::vector<Entry> entries_;
    c4::ID base_;
};

// Natural sort: numeric segments compared as integers.
// "file2.txt" < "file10.txt"
bool NaturalLess(const std::string &a, const std::string &b);

// Mode string conversion
std::string FormatMode(uint32_t mode);
uint32_t ParseMode(const std::string &s);

// Timestamp conversion (RFC3339 UTC)
std::string FormatTimestamp(int64_t unix_seconds);
int64_t ParseTimestamp(const std::string &s);

} // namespace c4m

#endif // C4_C4M_HPP
