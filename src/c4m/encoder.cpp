// SPDX-License-Identifier: Apache-2.0
// C4M format encoder — canonical output.
// Matches Go reference: github.com/Avalanche-io/c4/c4m/encoder.go

#include "c4/c4m.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace c4m {

std::string Manifest::Encode() const {
    // Make a mutable copy for sorting
    Manifest copy = *this;
    copy.SortEntries();

    std::string out;
    out.reserve(4096);

    // Header
    out += "@c4m ";
    out += copy.version_;
    out += '\n';

    // @base directive
    if (!copy.base_.IsNil()) {
        out += "@base ";
        out += copy.base_.String();
        out += '\n';
    }

    // Entries (canonical form with indentation)
    for (const auto &entry : copy.entries_) {
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
