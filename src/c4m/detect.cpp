// SPDX-License-Identifier: Apache-2.0
// c4m detection heuristic and canonicalization for c4m-aware identification.
// See design docs: c4m-detection.md, c4m-canonical-storage.md

#include "c4/c4m.hpp"

#include <string>
#include <string_view>

namespace c4m {

bool LooksLikeC4m(std::string_view data) {
    if (data.empty())
        return false;

    // Skip leading spaces (tabs are not valid c4m indentation)
    size_t pos = 0;
    while (pos < data.size() && data[pos] == ' ')
        pos++;

    if (pos >= data.size())
        return false;

    // Check first non-space byte for valid c4m mode characters
    char ch = data[pos];
    return ch == '-' || ch == 'd' || ch == 'l' ||
           ch == 'p' || ch == 's' || ch == 'b' || ch == 'c';
}

std::string CanonicalizeC4m(std::string_view data) {
    try {
        auto manifest = Manifest::Parse(data);
        if (manifest.EntryCount() == 0)
            return {};
        return manifest.Encode();
    } catch (...) {
        return {};
    }
}

} // namespace c4m
