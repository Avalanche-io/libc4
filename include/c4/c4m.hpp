// SPDX-License-Identifier: Apache-2.0
// C4M Format - C++ API
// Parser and encoder for .c4m files

#ifndef C4_C4M_HPP
#define C4_C4M_HPP

#include "c4.hpp"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c4m {

// A frame range for image sequences
struct FrameRange {
    int start;
    int end;
};

// A single entry in a .c4m manifest
struct Entry {
    enum class Type { File, Dir, Sequence };

    Type type = Type::File;
    std::string name;
    c4::ID id;
    int64_t size = 0;
    std::optional<FrameRange> frames;

    // For directories: child entries
    std::vector<Entry> children;

    bool IsDir() const { return type == Type::Dir; }
    bool IsSequence() const { return type == Type::Sequence; }
};

// A parsed .c4m manifest
class Manifest {
public:
    Manifest() = default;

    // Parse from string
    static Manifest Parse(std::string_view data);

    // Parse from file
    static Manifest ParseFile(const std::filesystem::path &path);

    // Parse from stream
    static Manifest Parse(std::istream &stream);

    // Scan a directory to build a manifest
    static Manifest Scan(const std::filesystem::path &dir);

    // Encode to .c4m format string
    std::string Encode() const;

    // Write to file
    void WriteFile(const std::filesystem::path &path) const;

    // Root ID of the manifest
    c4::ID RootID() const;

    // Access entries
    const std::vector<Entry> &Entries() const;
    size_t EntryCount() const;

    // Flat list of all entries (recursive)
    std::vector<const Entry *> AllEntries() const;

    // Diff: entries in this manifest that differ from other
    Manifest Diff(const Manifest &other) const;

    // Merge: combine two manifests
    Manifest Merge(const Manifest &other) const;

    // Intersect: entries common to both
    Manifest Intersect(const Manifest &other) const;

private:
    std::vector<Entry> entries_;
    c4::ID root_id_;
};

} // namespace c4m

#endif // C4_C4M_HPP
