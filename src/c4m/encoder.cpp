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

    // Entry-only output (no @c4m header, no @base directive).
    // This matches the Go reference encoder which produces entries only.
    for (const auto &entry : copy.Entries()) {
        out += entry.Format(2);
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
