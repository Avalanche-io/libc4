// SPDX-License-Identifier: Apache-2.0
// C4 Tree ID computation
//
// A tree ID is computed using a merkle tree. IDs are sorted, deduplicated,
// then paired bottom-up. Each pair is combined using Sum() (SHA-512 of
// smaller||larger). Odd elements pass through to the next level. This
// matches the Go reference implementation.

#include "c4/c4.hpp"
#include "internal.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace c4 {

void IDs::Append(const class ID &id) {
    ids_.push_back(id);
}

void IDs::Append(class ID &&id) {
    ids_.push_back(std::move(id));
}

c4::ID IDs::TreeID() const {
    if (ids_.empty()) {
        return c4::ID();
    }

    // Sort a copy and remove duplicates
    auto sorted = ids_;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    if (sorted.size() == 1) {
        return sorted[0];
    }

    // Build merkle tree bottom-up
    std::vector<c4::ID> current = std::move(sorted);
    while (current.size() > 1) {
        std::vector<c4::ID> next;
        next.reserve((current.size() + 1) / 2);

        size_t i = 0;
        for (; i + 1 < current.size(); i += 2) {
            next.push_back(current[i].Sum(current[i + 1]));
        }
        // Odd element passes through
        if (i < current.size()) {
            next.push_back(current[i]);
        }

        current = std::move(next);
    }

    return current[0];
}

size_t IDs::Size() const {
    return ids_.size();
}

bool IDs::Empty() const {
    return ids_.empty();
}

const c4::ID &IDs::operator[](size_t i) const {
    return ids_.at(i);
}

} // namespace c4

// C API
extern "C" {

c4_error_t c4_tree_id(const c4_id_t **ids, size_t count, c4_id_t *out) {
    if (!ids || !out) return C4_ERR_INVALID_INPUT;
    try {
        c4::IDs set;
        for (size_t i = 0; i < count; i++) {
            if (!ids[i]) return C4_ERR_INVALID_INPUT;
            set.Append(c4_id_from_raw(ids[i]->digest.data()));
        }
        auto result = set.TreeID();
        std::memcpy(out->digest.data(), result.Digest().data(), c4::DigestLen);
        return C4_OK;
    } catch (...) {
        return C4_ERR_IO;
    }
}

} // extern "C"
