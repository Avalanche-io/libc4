// SPDX-License-Identifier: Apache-2.0
// C4M format encoder — canonical output.
// Matches Go reference: github.com/Avalanche-io/c4/c4m/encoder.go

#include "c4/c4m.hpp"
#include "entry_line.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace c4m {

std::string Manifest::Encode() const {
    // A name the format forbids is never written: every decoder refuses such
    // a line (name-rule vector). Refused before anything is produced.
    for (const auto &e : entries_) {
        if (detail::isPathName(e.name))
            throw std::runtime_error("c4m: path traversal: name '" + e.name +
                                     "' is a path, not an entry name (only a directory's trailing / is allowed)");
    }
    // Hierarchical sort: Copy + SortEntries preserves depth-first tree structure.
    Manifest copy = Copy();
    copy.SortEntries();

    std::string out;
    out.reserve(4096);

    // A first-line bare ID is the external base this description refines
    // (SPECIFICATION.md, external base reference). Parse keeps it in Base();
    // writing it back keeps the round trip honest. Through 2026-11-13 it was
    // dropped, so encoding a decoded block turned it into a standalone
    // description of its own entries (c4 #145). A manifest with no base
    // encodes exactly as before: entries only, no @c4m header.
    if (!copy.Base().IsNil()) {
        out += copy.Base().String();
        out += '\n';
    }

    for (const auto &entry : copy.Entries()) {
        out += entry.Format(2);
        out += '\n';
    }

    // Inline range data trails the entries, sorted by the string form of
    // each list's C4 ID (Go's encoder sorts the same way). Through
    // 2026-11-13 Parse discarded these lines, so a folded sequence's ID list
    // was lost on the round trip (sequence-null-size vector, c4 7a6ba94).
    std::vector<std::pair<std::string, const std::string *>> lists;
    lists.reserve(copy.RangeData().size());
    for (const auto &kv : copy.RangeData())
        lists.emplace_back(kv.first.String(), &kv.second);
    std::sort(lists.begin(), lists.end());
    for (const auto &kv : lists) {
        out += *kv.second;
        out += '\n';
    }

    return out;
}

void Manifest::WriteFile(const std::filesystem::path &path) const {
    std::string data = Encode();
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("cannot open file for writing: " + path.string());
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
}

} // namespace c4m
