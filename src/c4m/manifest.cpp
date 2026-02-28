// SPDX-License-Identifier: Apache-2.0
// C4M manifest operations

#include "c4/c4m.hpp"

namespace c4m {

c4::ID Manifest::RootID() const {
    return root_id_;
}

const std::vector<Entry> &Manifest::Entries() const {
    return entries_;
}

size_t Manifest::EntryCount() const {
    return entries_.size();
}

std::vector<const Entry *> Manifest::AllEntries() const {
    std::vector<const Entry *> result;
    std::function<void(const std::vector<Entry> &)> walk =
        [&](const std::vector<Entry> &entries) {
            for (const auto &e : entries) {
                result.push_back(&e);
                if (e.IsDir()) {
                    walk(e.children);
                }
            }
        };
    walk(entries_);
    return result;
}

} // namespace c4m
