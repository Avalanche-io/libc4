// SPDX-License-Identifier: Apache-2.0
// C4M manifest: tree index, navigation, sorting, validation, ID computation.

#include "c4/c4m.hpp"

#include <algorithm>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace c4m {

// --------------------------------------------------------------------
// isPathName: true if name contains path semantics (not a bare filename).
// --------------------------------------------------------------------
static bool isPathName(const std::string &name) {
    if (name.empty())
        return true;
    std::string base = name;
    if (base.back() == '/')
        base.pop_back();
    if (base.empty())
        return true;
    if (base == "." || base == "..")
        return true;
    for (char c : base) {
        if (c == '/' || c == '\\' || c == '\0')
            return true;
    }
    return false;
}

// --------------------------------------------------------------------
// globMatch: simple glob supporting * and ?
// --------------------------------------------------------------------
static bool globMatch(const std::string &pattern, const std::string &str) {
    size_t pi = 0, si = 0;
    size_t starP = std::string::npos, starS = 0;
    while (si < str.size()) {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == str[si])) {
            pi++;
            si++;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            starP = pi++;
            starS = si;
        } else if (starP != std::string::npos) {
            pi = starP + 1;
            si = ++starS;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*')
        pi++;
    return pi == pattern.size();
}

// ====================================================================
// AllEntries
// ====================================================================

std::vector<const Entry *> Manifest::AllEntries() const {
    std::vector<const Entry *> result;
    result.reserve(entries_.size());
    for (const auto &e : entries_)
        result.push_back(&e);
    return result;
}

// ====================================================================
// Tree Index
// ====================================================================

void Manifest::invalidateIndex() {
    index_.reset();
}

void Manifest::InvalidateIndex() {
    invalidateIndex();
}

const TreeIndex &Manifest::ensureIndex() const {
    if (index_)
        return *index_;

    auto idx = std::make_unique<TreeIndex>();

    // Pass 1: bare-name index and root entries.
    for (const auto &e : entries_) {
        idx->by_name[e.name] = &e;
        if (e.depth == 0)
            idx->root.push_back(&e);
    }

    // Pass 2: parent-child relationships.
    for (size_t i = 0; i < entries_.size(); i++) {
        const Entry *e = &entries_[i];
        if (e->depth == 0)
            continue;
        for (size_t j = i; j-- > 0;) {
            const Entry *candidate = &entries_[j];
            if (candidate->depth == e->depth - 1 && candidate->IsDir()) {
                idx->parent[e] = candidate;
                idx->children[candidate].push_back(e);
                break;
            }
            if (candidate->depth < e->depth - 1)
                break;
        }
    }

    // Pass 3: compute full paths from parent chain.
    for (const auto &e : entries_) {
        const Entry *cur = &e;
        std::vector<std::string> parts;
        while (cur) {
            parts.push_back(cur->name);
            auto it = idx->parent.find(cur);
            cur = (it != idx->parent.end()) ? it->second : nullptr;
        }
        std::string fullPath;
        for (auto rit = parts.rbegin(); rit != parts.rend(); ++rit)
            fullPath += *rit;
        idx->by_path[fullPath] = &e;
        idx->path_of[&e] = fullPath;
    }

    index_ = std::move(idx);
    return *index_;
}

// ====================================================================
// Navigation
// ====================================================================

const Entry *Manifest::GetEntry(const std::string &path) const {
    const auto &idx = ensureIndex();
    auto it = idx.by_path.find(path);
    return (it != idx.by_path.end()) ? it->second : nullptr;
}

const Entry *Manifest::GetEntryByName(const std::string &name) const {
    const auto &idx = ensureIndex();
    auto it = idx.by_name.find(name);
    return (it != idx.by_name.end()) ? it->second : nullptr;
}

std::string Manifest::EntryPath(const Entry *e) const {
    if (!e)
        return "";
    const auto &idx = ensureIndex();
    auto it = idx.path_of.find(e);
    return (it != idx.path_of.end()) ? it->second : "";
}

std::vector<const Entry *> Manifest::Children(const Entry *e) const {
    if (!e || !e->IsDir())
        return {};
    const auto &idx = ensureIndex();
    auto it = idx.children.find(e);
    return (it != idx.children.end()) ? it->second : std::vector<const Entry *>{};
}

const Entry *Manifest::Parent(const Entry *e) const {
    if (!e || e->depth == 0)
        return nullptr;
    const auto &idx = ensureIndex();
    auto it = idx.parent.find(e);
    return (it != idx.parent.end()) ? it->second : nullptr;
}

std::vector<const Entry *> Manifest::Siblings(const Entry *e) const {
    if (!e)
        return {};
    const auto &idx = ensureIndex();
    auto pit = idx.parent.find(e);
    const Entry *par = (pit != idx.parent.end()) ? pit->second : nullptr;

    std::vector<const Entry *> result;
    if (!par) {
        for (const Entry *r : idx.root) {
            if (r != e)
                result.push_back(r);
        }
    } else {
        auto cit = idx.children.find(par);
        if (cit != idx.children.end()) {
            for (const Entry *c : cit->second) {
                if (c != e)
                    result.push_back(c);
            }
        }
    }
    return result;
}

std::vector<const Entry *> Manifest::Ancestors(const Entry *e) const {
    if (!e || e->depth == 0)
        return {};
    const auto &idx = ensureIndex();
    std::vector<const Entry *> result;
    auto it = idx.parent.find(e);
    while (it != idx.parent.end()) {
        result.push_back(it->second);
        it = idx.parent.find(it->second);
    }
    return result;
}

std::vector<const Entry *> Manifest::Descendants(const Entry *e) const {
    if (!e || !e->IsDir())
        return {};
    const auto &idx = ensureIndex();
    std::vector<const Entry *> result;
    std::function<void(const Entry *)> collect = [&](const Entry *parent) {
        auto it = idx.children.find(parent);
        if (it == idx.children.end())
            return;
        for (const Entry *child : it->second) {
            result.push_back(child);
            if (child->IsDir())
                collect(child);
        }
    };
    collect(e);
    return result;
}

std::vector<const Entry *> Manifest::Root() const {
    const auto &idx = ensureIndex();
    return idx.root;
}

std::vector<const Entry *> Manifest::GetEntriesAtDepth(int depth) const {
    std::vector<const Entry *> result;
    for (const auto &e : entries_) {
        if (e.depth == depth)
            result.push_back(&e);
    }
    return result;
}

std::vector<std::string> Manifest::PathList() const {
    const auto &idx = ensureIndex();
    std::vector<std::string> paths;
    paths.reserve(entries_.size());
    for (const auto &e : entries_) {
        auto it = idx.path_of.find(&e);
        if (it != idx.path_of.end())
            paths.push_back(it->second);
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

// ====================================================================
// Mutators
// ====================================================================

void Manifest::AddEntry(Entry e) {
    entries_.push_back(std::move(e));
    invalidateIndex();
}

void Manifest::RemoveEntry(const Entry *e) {
    if (!e)
        return;
    std::unordered_set<const Entry *> toRemove;
    toRemove.insert(e);
    if (e->IsDir()) {
        for (const Entry *d : Descendants(e))
            toRemove.insert(d);
    }
    std::vector<Entry> kept;
    kept.reserve(entries_.size());
    for (const auto &entry : entries_) {
        if (toRemove.find(&entry) == toRemove.end())
            kept.push_back(entry);
    }
    entries_ = std::move(kept);
    invalidateIndex();
}

void Manifest::MoveEntry(const Entry *e, const Entry *new_parent,
                          const std::string &new_name) {
    if (!e)
        return;
    int target_depth = new_parent ? new_parent->depth + 1 : 0;
    int depth_delta = target_depth - e->depth;

    std::vector<const Entry *> descs;
    if (e->IsDir())
        descs = Descendants(e);

    for (auto &entry : entries_) {
        if (&entry == e) {
            entry.name = new_name;
            entry.depth = target_depth;
            break;
        }
    }
    for (const Entry *d : descs) {
        for (auto &entry : entries_) {
            if (&entry == d) {
                entry.depth += depth_delta;
                break;
            }
        }
    }
    invalidateIndex();
    SortEntries();
}

// ====================================================================
// Copy
// ====================================================================

Manifest Manifest::Copy() const {
    Manifest cp;
    cp.version_ = version_;
    cp.base_ = base_;
    cp.entries_ = entries_;
    return cp;
}

// ====================================================================
// Canonicalize
// ====================================================================

void Manifest::Canonicalize() {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        Entry &entry = *it;
        if (!entry.IsDir() || !entry.HasNullValues())
            continue;

        auto dir_idx = static_cast<size_t>(&entry - &entries_[0]);
        std::vector<const Entry *> kids;
        for (size_t j = dir_idx + 1; j < entries_.size(); j++) {
            if (entries_[j].depth == entry.depth + 1)
                kids.push_back(&entries_[j]);
            else if (entries_[j].depth <= entry.depth)
                break;
        }

        // Empty directory: size is definitively 0.
        if (kids.empty()) {
            if (entry.size < 0)
                entry.size = 0;
            continue;
        }

        if (entry.size < 0) {
            int64_t total = 0;
            bool has_null = false;
            for (const Entry *k : kids) {
                if (k->size < 0) { has_null = true; break; }
                total += k->size;
            }
            if (!has_null) {
                // Add byte length of the directory's canonical c4m content
                for (const Entry *k : kids) {
                    total += static_cast<int64_t>(k->Canonical().size()) + 1; // +1 for '\n'
                }
            }
            entry.size = has_null ? -1 : total;
        }

        if (entry.timestamp == NullTimestamp) {
            int64_t most_recent = 0;
            bool has_null = false;
            for (const Entry *k : kids) {
                if (k->timestamp == NullTimestamp) { has_null = true; break; }
                if (k->timestamp > most_recent) most_recent = k->timestamp;
            }
            entry.timestamp = has_null ? NullTimestamp : most_recent;
        }
    }
}

// ====================================================================
// ComputeC4ID
// ====================================================================

c4::ID Manifest::ComputeC4ID() const {
    Manifest copy = Copy();
    copy.Canonicalize();
    copy.SortEntries();

    std::string canonical;
    for (const auto &entry : copy.entries_) {
        if (entry.depth == 0) {
            canonical += entry.Canonical();
            canonical += '\n';
        }
    }
    if (canonical.empty())
        return c4::ID();
    return c4::ID::Identify(canonical);
}

c4::ID Manifest::RootID() const {
    return ComputeC4ID();
}

// ====================================================================
// SortEntries
// ====================================================================

void Manifest::SortEntries() {
    if (entries_.empty())
        return;

    std::vector<Entry> result;
    result.reserve(entries_.size());
    std::vector<bool> used(entries_.size(), false);

    std::function<void(int, int)> processLevel;
    processLevel = [&](int parent_idx, int parent_depth) {
        int child_depth = parent_depth + 1;
        size_t start_idx = (parent_idx < 0) ? 0 : static_cast<size_t>(parent_idx + 1);
        if (parent_idx < 0)
            child_depth = 0;

        struct Child { size_t index; const Entry *entry; };
        std::vector<Child> children;

        for (size_t i = start_idx; i < entries_.size(); i++) {
            if (used[i]) continue;
            const auto &entry = entries_[i];
            if (entry.depth < child_depth) break;
            if (entry.depth > child_depth) continue;
            children.push_back({i, &entry});
        }

        // Deduplicate siblings by name (last occurrence wins).
        {
            std::unordered_map<std::string, size_t> seen;
            std::vector<Child> deduped;
            for (auto &c : children) {
                auto it = seen.find(c.entry->name);
                if (it != seen.end()) {
                    used[deduped[it->second].index] = true;
                    deduped[it->second] = c;
                } else {
                    seen[c.entry->name] = deduped.size();
                    deduped.push_back(c);
                }
            }
            children = std::move(deduped);
        }

        std::sort(children.begin(), children.end(), [](const Child &a, const Child &b) {
            bool a_dir = a.entry->IsDir();
            bool b_dir = b.entry->IsDir();
            if (a_dir != b_dir) return !a_dir;
            return NaturalLess(a.entry->name, b.entry->name);
        });

        for (const auto &child : children) {
            used[child.index] = true;
            result.push_back(entries_[child.index]);
            if (entries_[child.index].IsDir())
                processLevel(static_cast<int>(child.index), entries_[child.index].depth);
        }
    };

    processLevel(-1, -1);

    for (size_t i = 0; i < entries_.size(); i++) {
        if (!used[i])
            result.push_back(entries_[i]);
    }

    entries_ = std::move(result);
    invalidateIndex();
}

// ====================================================================
// Validate
// ====================================================================

void Manifest::Validate() const {
    if (version_.empty())
        throw std::runtime_error("c4m: missing version");

    std::unordered_set<std::string> seen;
    std::vector<std::string> dirStack;

    for (const auto &e : entries_) {
        if (e.name.empty())
            throw std::runtime_error("c4m: empty name");
        if (isPathName(e.name))
            throw std::runtime_error("c4m: path traversal: " + e.name);

        if (static_cast<int>(dirStack.size()) > e.depth)
            dirStack.resize(static_cast<size_t>(e.depth));

        std::string fullPath;
        for (const auto &part : dirStack)
            fullPath += part;
        fullPath += e.name;

        if (seen.count(fullPath))
            throw std::runtime_error("c4m: duplicate path: " + fullPath);
        seen.insert(fullPath);

        if (e.IsDir()) {
            while (static_cast<int>(dirStack.size()) <= e.depth)
                dirStack.emplace_back();
            dirStack[static_cast<size_t>(e.depth)] = e.name;
        }
    }
}

// ====================================================================
// Filtering
// ====================================================================

Manifest Manifest::FilterByPath(const std::string &pattern) const {
    Manifest result;
    result.version_ = version_;
    for (const auto &entry : entries_) {
        if (globMatch(pattern, entry.name))
            result.entries_.push_back(entry);
    }
    return result;
}

Manifest Manifest::FilterByPrefix(const std::string &prefix) const {
    Manifest result;
    result.version_ = version_;
    const auto &idx = ensureIndex();
    for (const auto &entry : entries_) {
        auto it = idx.path_of.find(&entry);
        if (it != idx.path_of.end() &&
            it->second.compare(0, prefix.size(), prefix) == 0) {
            result.entries_.push_back(entry);
        }
    }
    return result;
}

} // namespace c4m
