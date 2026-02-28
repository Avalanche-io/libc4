// SPDX-License-Identifier: Apache-2.0
// C4M manifest: accessors, sorting, validation, ID computation.

#include "c4/c4m.hpp"

#include <algorithm>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace c4m {

std::vector<const Entry *> Manifest::AllEntries() const {
    std::vector<const Entry *> result;
    result.reserve(entries_.size());
    for (const auto &e : entries_)
        result.push_back(&e);
    return result;
}

const Entry *Manifest::GetEntry(const std::string &name) const {
    for (const auto &e : entries_) {
        if (e.name == name)
            return &e;
    }
    return nullptr;
}

std::vector<const Entry *> Manifest::GetEntriesAtDepth(int depth) const {
    std::vector<const Entry *> result;
    for (const auto &e : entries_) {
        if (e.depth == depth)
            result.push_back(&e);
    }
    return result;
}

void Manifest::AddEntry(Entry e) {
    entries_.push_back(std::move(e));
}

c4::ID Manifest::RootID() const {
    // Compute canonical form text and identify it
    std::string canonical;
    for (const auto &entry : entries_) {
        if (entry.depth == 0) {
            canonical += entry.Canonical();
            canonical += '\n';
        }
    }
    if (canonical.empty())
        return c4::ID();
    return c4::ID::Identify(canonical);
}

void Manifest::SortEntries() {
    if (entries_.empty())
        return;

    // Hierarchical sort: preserve depth-first structure, sort siblings.
    // Algorithm: collect children at each level, sort them (files before dirs,
    // natural sort), then recurse into directory children.

    std::vector<Entry> result;
    result.reserve(entries_.size());
    std::vector<bool> used(entries_.size(), false);

    // Recursive lambda to process one level
    std::function<void(int parent_idx, int parent_depth)> processLevel;
    processLevel = [&](int parent_idx, int parent_depth) {
        int child_depth = parent_depth + 1;
        size_t start_idx = (parent_idx < 0) ? 0 : static_cast<size_t>(parent_idx + 1);

        if (parent_idx < 0) {
            child_depth = 0;
        }

        // Collect immediate children
        struct Child {
            size_t index;
            const Entry *entry;
        };
        std::vector<Child> children;

        for (size_t i = start_idx; i < entries_.size(); i++) {
            if (used[i]) continue;

            const auto &entry = entries_[i];
            if (entry.depth < child_depth) break;
            if (entry.depth > child_depth) continue;

            children.push_back({i, &entry});
        }

        // Sort: files before dirs, then natural sort
        std::sort(children.begin(), children.end(), [](const Child &a, const Child &b) {
            bool a_dir = a.entry->IsDir();
            bool b_dir = b.entry->IsDir();
            if (a_dir != b_dir) return !a_dir; // files first
            return NaturalLess(a.entry->name, b.entry->name);
        });

        // Process sorted children
        for (const auto &child : children) {
            used[child.index] = true;
            result.push_back(entries_[child.index]);

            if (entries_[child.index].IsDir()) {
                processLevel(static_cast<int>(child.index), entries_[child.index].depth);
            }
        }
    };

    processLevel(-1, -1);

    // Add any orphaned entries
    for (size_t i = 0; i < entries_.size(); i++) {
        if (!used[i])
            result.push_back(entries_[i]);
    }

    entries_ = std::move(result);
}

void Manifest::Validate() const {
    if (version_.empty())
        throw std::runtime_error("c4m: missing version");

    std::unordered_set<std::string> seen;
    for (const auto &e : entries_) {
        if (e.name.empty())
            throw std::runtime_error("c4m: empty name");

        if (seen.count(e.name))
            throw std::runtime_error("c4m: duplicate path: " + e.name);
        seen.insert(e.name);

        // Path traversal check
        if (e.name.find("../") != std::string::npos ||
            e.name.find("./") != std::string::npos)
            throw std::runtime_error("c4m: path traversal: " + e.name);
    }
}

} // namespace c4m
