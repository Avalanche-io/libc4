// SPDX-License-Identifier: Apache-2.0
// C4M operations: PatchDiff, ApplyPatch, Diff, EntryPaths, EncodePatch,
// DecodePatchChain, ResolvePatchChain, Merge.
// Reference: Go implementation at github.com/Avalanche-io/c4/c4m

#include "c4/c4m.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace c4m {

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

// entriesIdentical checks exact equality across all metadata fields.
// Used by patch semantics: an exact duplicate signals removal.
static bool entriesIdentical(const Entry &a, const Entry &b) {
    return a.name == b.name &&
           a.mode == b.mode &&
           a.timestamp == b.timestamp &&
           a.size == b.size &&
           a.id == b.id &&
           a.target == b.target &&
           a.hard_link == b.hard_link &&
           a.flow_direction == b.flow_direction &&
           a.flow_target == b.flow_target;
}

// entriesEqual compares two entries for Diff equality.
// Weaker than entriesIdentical — only cares about content equivalence.
static bool entriesEqual(const Entry &a, const Entry &b) {
    if (a.name != b.name)
        return false;
    if (!a.id.IsNil() && !b.id.IsNil())
        return a.id == b.id && a.mode == b.mode;
    return a.mode == b.mode &&
           a.size == b.size &&
           a.timestamp == b.timestamp &&
           a.target == b.target;
}

// mergeEqual checks content equality for three-way merge.
static bool mergeEqual(const Entry &a, const Entry &b) {
    if (a.IsDir() != b.IsDir())
        return false;
    if (a.IsDir())
        return a.flow_direction == b.flow_direction && a.flow_target == b.flow_target;
    if (a.IsSymlink() || b.IsSymlink())
        return a.IsSymlink() == b.IsSymlink() && a.target == b.target;
    return a.id == b.id;
}

// Entry at a given depth (copy with depth set).
static Entry entryAtDepth(const Entry &src, int depth) {
    Entry e = src;
    e.depth = depth;
    return e;
}

// getDirectoryChildren returns immediate children of a directory entry
// by scanning the flat entry list.
static std::vector<const Entry *> getDirectoryChildren(
    const std::vector<Entry> &entries, const Entry *dir)
{
    std::vector<const Entry *> children;
    int dir_depth = dir->depth;
    bool collecting = false;

    for (const auto &e : entries) {
        if (&e == dir) {
            collecting = true;
            continue;
        }
        if (collecting) {
            if (e.depth == dir_depth + 1)
                children.push_back(&e);
            else if (e.depth <= dir_depth)
                break;
        }
    }
    return children;
}

// emitSubtree recursively emits all children of a directory into result.
static void emitSubtree(const std::vector<Entry> &entries, const Entry *dir,
                         int depth, std::vector<Entry> &result)
{
    for (const auto *child : getDirectoryChildren(entries, dir)) {
        result.push_back(entryAtDepth(*child, depth));
        if (child->IsDir())
            emitSubtree(entries, child, depth + 1, result);
    }
}

// diffUnionNames returns sorted union of entry names: files before dirs, then lex.
static std::vector<std::string> diffUnionNames(
    const std::unordered_map<std::string, const Entry *> &a,
    const std::unordered_map<std::string, const Entry *> &b)
{
    std::set<std::string> seen;
    for (const auto &p : a) seen.insert(p.first);
    for (const auto &p : b) seen.insert(p.first);

    std::vector<std::string> files, dirs;
    for (const auto &name : seen) {
        bool is_dir = false;
        auto it = a.find(name);
        if (it != a.end() && it->second->IsDir()) is_dir = true;
        it = b.find(name);
        if (it != b.end() && it->second->IsDir()) is_dir = true;

        if (is_dir)
            dirs.push_back(name);
        else
            files.push_back(name);
    }
    std::sort(files.begin(), files.end());
    std::sort(dirs.begin(), dirs.end());
    files.insert(files.end(), dirs.begin(), dirs.end());
    return files;
}

// diffTree recursively compares children at a given depth, emitting patch entries.
static void diffTree(const std::vector<Entry> &old_entries,
                     const std::vector<Entry> &new_entries,
                     const std::vector<const Entry *> &old_children,
                     const std::vector<const Entry *> &new_children,
                     int depth,
                     std::vector<Entry> &result)
{
    std::unordered_map<std::string, const Entry *> old_by_name;
    for (const auto *e : old_children)
        old_by_name[e->name] = e;
    std::unordered_map<std::string, const Entry *> new_by_name;
    for (const auto *e : new_children)
        new_by_name[e->name] = e;

    auto names = diffUnionNames(old_by_name, new_by_name);

    for (const auto &name : names) {
        auto old_it = old_by_name.find(name);
        auto new_it = new_by_name.find(name);
        const Entry *old_entry = (old_it != old_by_name.end()) ? old_it->second : nullptr;
        const Entry *new_entry = (new_it != new_by_name.end()) ? new_it->second : nullptr;

        if (new_entry && !old_entry) {
            // Addition: emit new entry and full subtree
            result.push_back(entryAtDepth(*new_entry, depth));
            if (new_entry->IsDir())
                emitSubtree(new_entries, new_entry, depth + 1, result);
            continue;
        }

        if (old_entry && !new_entry) {
            // Removal: re-emit old entry (exact duplicate signals removal)
            result.push_back(entryAtDepth(*old_entry, depth));
            continue;
        }

        // Both exist
        if (entriesIdentical(*old_entry, *new_entry)) {
            if (!old_entry->IsDir()) {
                // Identical file: skip
                continue;
            }
            // Identical directory entry: children may differ, so recurse
            auto old_kids = getDirectoryChildren(old_entries, old_entry);
            auto new_kids = getDirectoryChildren(new_entries, new_entry);
            std::vector<Entry> child_entries;
            diffTree(old_entries, new_entries, old_kids, new_kids, depth + 1, child_entries);
            if (child_entries.empty())
                continue; // No child differences
            // Children differ: emit dir entry and child diffs
            result.push_back(entryAtDepth(*new_entry, depth));
            result.insert(result.end(), child_entries.begin(), child_entries.end());
            continue;
        }

        if (old_entry->IsDir() && new_entry->IsDir()) {
            // Both directories, different content: emit new dir and recurse
            result.push_back(entryAtDepth(*new_entry, depth));
            auto old_kids = getDirectoryChildren(old_entries, old_entry);
            auto new_kids = getDirectoryChildren(new_entries, new_entry);
            diffTree(old_entries, new_entries, old_kids, new_kids, depth + 1, result);
        } else {
            // File modified or type changed: emit new entry (clobber)
            result.push_back(entryAtDepth(*new_entry, depth));
        }
    }
}

// -----------------------------------------------------------------------
// PatchDiff
// -----------------------------------------------------------------------

PatchResult PatchDiff(const Manifest &old_m, const Manifest &new_m) {
    // Get root children from each manifest
    std::vector<const Entry *> old_root, new_root;
    for (const auto &e : old_m.Entries()) {
        if (e.depth == 0)
            old_root.push_back(&e);
    }
    for (const auto &e : new_m.Entries()) {
        if (e.depth == 0)
            new_root.push_back(&e);
    }

    std::vector<Entry> entries;
    diffTree(old_m.Entries(), new_m.Entries(), old_root, new_root, 0, entries);

    PatchResult pr;
    for (auto &e : entries)
        pr.patch.AddEntry(std::move(e));
    pr.oldID = old_m.ComputeC4ID();
    pr.newID = new_m.ComputeC4ID();
    return pr;
}

// -----------------------------------------------------------------------
// Patch tree for ApplyPatch
// -----------------------------------------------------------------------

struct PatchNode {
    Entry entry;
    std::map<std::string, PatchNode> children;
};

static PatchNode buildPatchTree(const Manifest &m) {
    PatchNode root;
    std::vector<PatchNode *> stack;
    stack.push_back(&root);

    for (const auto &e : m.Entries()) {
        // Trim stack to depth+1
        if (e.depth + 1 < static_cast<int>(stack.size()))
            stack.resize(static_cast<size_t>(e.depth + 1));

        PatchNode *parent = stack[static_cast<size_t>(e.depth)];

        PatchNode node;
        node.entry = e;
        auto &ref = parent->children[e.name] = std::move(node);

        if (e.IsDir()) {
            while (static_cast<int>(stack.size()) <= e.depth + 1)
                stack.push_back(nullptr);
            stack[static_cast<size_t>(e.depth + 1)] = &ref;
        }
    }
    return root;
}

static void applyPatchTree(PatchNode &base, const PatchNode &patch) {
    for (const auto &kv : patch.children) {
        const auto &name = kv.first;
        const auto &p_node = kv.second;

        auto b_it = base.children.find(name);

        if (b_it == base.children.end()) {
            // Addition: graft entire subtree
            base.children[name] = p_node;
            continue;
        }

        auto &b_node = b_it->second;

        // Check if exact match (removal)
        if (entriesIdentical(b_node.entry, p_node.entry)) {
            base.children.erase(b_it);
            continue;
        }

        // Clobber: replace entry
        b_node.entry = p_node.entry;

        // For directories, recurse to apply child-level changes
        if (b_node.entry.IsDir() && p_node.entry.IsDir())
            applyPatchTree(b_node, p_node);
    }
}

static void flattenPatchTree(const PatchNode &node, int depth, std::vector<Entry> &result) {
    for (const auto &kv : node.children) {
        Entry e = kv.second.entry;
        e.depth = depth;
        result.push_back(e);
        if (kv.second.entry.IsDir())
            flattenPatchTree(kv.second, depth + 1, result);
    }
}

Manifest ApplyPatch(const Manifest &base, const Manifest &patch) {
    PatchNode base_tree = buildPatchTree(base);
    PatchNode patch_tree = buildPatchTree(patch);
    applyPatchTree(base_tree, patch_tree);

    std::vector<Entry> entries;
    flattenPatchTree(base_tree, 0, entries);

    Manifest result;
    for (auto &e : entries)
        result.AddEntry(std::move(e));
    result.SortEntries();
    return result;
}

// -----------------------------------------------------------------------
// Diff
// -----------------------------------------------------------------------

DiffResult Diff(const Manifest &a, const Manifest &b) {
    // Build name maps
    std::unordered_map<std::string, const Entry *> a_map, b_map;
    for (const auto &e : a.Entries())
        a_map[e.name] = &e;
    for (const auto &e : b.Entries())
        b_map[e.name] = &e;

    DiffResult result;

    // Check entries in A
    for (const auto &kv : a_map) {
        auto b_it = b_map.find(kv.first);
        if (b_it != b_map.end()) {
            if (entriesEqual(*kv.second, *b_it->second))
                result.same.AddEntry(*kv.second);
            else
                result.modified.AddEntry(*b_it->second);
        } else {
            result.removed.AddEntry(*kv.second);
        }
    }

    // Check entries only in B (added)
    for (const auto &kv : b_map) {
        if (a_map.find(kv.first) == a_map.end())
            result.added.AddEntry(*kv.second);
    }

    result.added.SortEntries();
    result.removed.SortEntries();
    result.modified.SortEntries();
    result.same.SortEntries();

    return result;
}

// -----------------------------------------------------------------------
// EntryPaths
// -----------------------------------------------------------------------

std::map<std::string, const Entry *> EntryPaths(const std::vector<Entry> &entries) {
    std::map<std::string, const Entry *> result;
    std::vector<std::string> stack;

    for (const auto &e : entries) {
        while (static_cast<int>(stack.size()) > e.depth)
            stack.pop_back();

        std::string full_path;
        for (const auto &s : stack)
            full_path += s;
        full_path += e.name;

        result[full_path] = &e;

        if (e.IsDir())
            stack.push_back(e.name);
    }

    return result;
}

// -----------------------------------------------------------------------
// EncodePatch
// -----------------------------------------------------------------------

std::string EncodePatch(const PatchResult &pr) {
    std::string out;
    out.reserve(4096);

    // Old ID (page boundary)
    if (!pr.oldID.IsNil()) {
        out += pr.oldID.String();
        out += '\n';
    }

    // Patch entries
    for (const auto &entry : pr.patch.Entries()) {
        out += entry.Format(2);
        out += '\n';
    }

    // New ID (page boundary)
    if (!pr.newID.IsNil()) {
        out += pr.newID.String();
        out += '\n';
    }

    return out;
}

// -----------------------------------------------------------------------
// DecodePatchChain
// -----------------------------------------------------------------------

// isBareC4ID: exactly 90 chars starting with "c4"
static bool isBareC4ID(const std::string &s) {
    return s.size() == 90 && s[0] == 'c' && s[1] == '4';
}

// isInlineIDList: length > 90, multiple of 90, starts with "c4"
static bool isInlineIDList(const std::string &s) {
    size_t n = s.size();
    if (n <= 90 || n % 90 != 0 || s[0] != 'c' || s[1] != '4')
        return false;
    // Verify each 90-char chunk starts with "c4"
    for (size_t i = 0; i < n; i += 90) {
        if (s[i] != 'c' || s[i + 1] != '4')
            return false;
    }
    return true;
}

// readLine: read one line, strip \r
static bool readChainLine(std::istream &in, std::string &line) {
    if (!std::getline(in, line))
        return false;
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return true;
}

// Minimal entry parser for patch chain decoding.
// Parses a content line (mode timestamp size name [-> target] [c4id|-]).
// This re-uses the existing parser logic but operates on a single line
// with known indentation context.
static Entry parseEntryLine(const std::string &line, int indent_width) {
    // Detect indentation
    size_t indent = 0;
    while (indent < line.size() && line[indent] == ' ')
        indent++;

    int depth = 0;
    if (indent_width > 0 && indent > 0)
        depth = static_cast<int>(indent) / indent_width;

    std::string content = line.substr(indent);
    if (content.empty())
        throw std::runtime_error("c4m: empty entry line");

    // Parse mode
    size_t pos = 0;
    uint32_t mode = 0;

    if (content.size() >= 2 && content[0] == '-' && content[1] == ' ') {
        // Null mode (single dash)
        pos = 2;
    } else if (content.size() >= 11) {
        std::string mode_str = content.substr(0, 10);
        if (mode_str != "-" && mode_str != "----------")
            mode = ParseMode(mode_str);
        pos = 11;
    } else {
        throw std::runtime_error("c4m: entry line too short");
    }

    Entry entry;
    entry.mode = mode;
    entry.depth = depth;

    // Parse timestamp
    auto skip = [&]() {
        while (pos < content.size() && content[pos] == ' ') pos++;
    };

    if (pos >= content.size())
        throw std::runtime_error("c4m: missing timestamp");

    if (content[pos] == '-' && (pos + 1 >= content.size() || content[pos + 1] == ' ')) {
        entry.timestamp = NullTimestamp;
        pos += 2;
    } else if (content[pos] == '0' && (pos + 1 >= content.size() || content[pos + 1] == ' ')) {
        entry.timestamp = NullTimestamp;
        pos += 2;
    } else if (content.size() >= pos + 20 && content[pos + 4] == '-' && content[pos + 10] == 'T') {
        size_t end_idx = pos + 20;
        if (content.size() >= pos + 25 &&
            (content[pos + 19] == '+' || content[pos + 19] == '-'))
            end_idx = pos + 25;
        entry.timestamp = ParseTimestamp(content.substr(pos, end_idx - pos));
        pos = end_idx;
        if (pos < content.size() && content[pos] == ' ') pos++;
    } else {
        throw std::runtime_error("c4m: cannot parse timestamp");
    }

    // Parse size
    skip();
    if (pos >= content.size())
        throw std::runtime_error("c4m: missing size");

    if (content[pos] == '-' && (pos + 1 >= content.size() || content[pos + 1] == ' ')) {
        entry.size = NullSize;
        pos++;
    } else {
        size_t size_start = pos;
        while (pos < content.size() &&
               ((content[pos] >= '0' && content[pos] <= '9') || content[pos] == ','))
            pos++;
        std::string size_str;
        for (size_t i = size_start; i < pos; i++) {
            if (content[i] != ',')
                size_str += content[i];
        }
        entry.size = std::stoll(size_str);
    }

    skip();

    // Parse name
    if (pos >= content.size())
        throw std::runtime_error("c4m: missing name");

    // Simplified name parser (handles quoted and unquoted)
    if (content[pos] == '"') {
        pos++; // skip opening quote
        std::string buf;
        while (pos < content.size()) {
            char ch = content[pos];
            if (ch == '\\' && pos + 1 < content.size()) {
                char next = content[pos + 1];
                if (next == '\\') buf += '\\';
                else if (next == '"') buf += '"';
                else if (next == 'n') buf += '\n';
                else { buf += '\\'; buf += next; }
                pos += 2;
            } else if (ch == '"') {
                pos++;
                break;
            } else {
                buf += ch;
                pos++;
            }
        }
        entry.name = buf;
    } else {
        size_t start = pos;
        while (pos < content.size()) {
            char ch = content[pos];
            if (ch == '/') {
                pos++;
                entry.name = content.substr(start, pos - start);
                goto name_done;
            }
            if (ch == ' ' && pos + 1 < content.size()) {
                if (content.size() - pos >= 4 && content.substr(pos, 4) == " -> ") {
                    entry.name = content.substr(start, pos - start);
                    goto name_done;
                }
                if (content[pos + 1] == 'c' && pos + 2 < content.size() && content[pos + 2] == '4') {
                    entry.name = content.substr(start, pos - start);
                    goto name_done;
                }
                if (content[pos + 1] == '-' && (pos + 2 >= content.size() || content[pos + 2] == ' ')) {
                    entry.name = content.substr(start, pos - start);
                    goto name_done;
                }
            }
            pos++;
        }
        entry.name = content.substr(start);
    }
name_done:

    skip();

    // Check for symlink target
    if (pos + 2 < content.size() && content[pos] == '-' && content[pos + 1] == '>') {
        pos += 2;
        skip();
        // Parse target (simplified)
        if (pos < content.size() && content[pos] == '"') {
            pos++;
            std::string buf;
            while (pos < content.size()) {
                char ch = content[pos];
                if (ch == '\\' && pos + 1 < content.size()) {
                    char next = content[pos + 1];
                    if (next == '\\') buf += '\\';
                    else if (next == '"') buf += '"';
                    else if (next == 'n') buf += '\n';
                    else { buf += '\\'; buf += next; }
                    pos += 2;
                } else if (ch == '"') {
                    pos++;
                    break;
                } else {
                    buf += ch;
                    pos++;
                }
            }
            entry.target = buf;
        } else {
            size_t start = pos;
            while (pos < content.size()) {
                char ch = content[pos];
                if (ch == ' ' && pos + 1 < content.size()) {
                    if (content[pos + 1] == 'c' && pos + 2 < content.size() && content[pos + 2] == '4')
                        break;
                    if (content[pos + 1] == '-' && (pos + 2 >= content.size() || content[pos + 2] == ' '))
                        break;
                }
                pos++;
            }
            entry.target = content.substr(start, pos - start);
        }
        skip();
    }

    // Check for C4 ID
    if (pos < content.size()) {
        std::string remaining = content.substr(pos);
        while (!remaining.empty() && remaining.back() == ' ')
            remaining.pop_back();
        if (remaining == "-") {
            // Null C4 ID
        } else if (remaining.size() >= 2 && remaining[0] == 'c' && remaining[1] == '4') {
            entry.id = c4::ID::Parse(remaining);
        }
    }

    return entry;
}

std::vector<PatchSection> DecodePatchChain(std::istream &input) {
    std::vector<PatchSection> sections;
    PatchSection current;
    bool first_line = true;
    int indent_width = -1;
    std::string line;

    while (readChainLine(input, line)) {
        // Trim
        std::string trimmed = line;
        size_t first = trimmed.find_first_not_of(' ');
        if (first == std::string::npos)
            continue; // blank line
        trimmed = trimmed.substr(first);

        if (trimmed.empty())
            continue;

        // Skip inline ID lists (range data)
        if (isInlineIDList(trimmed))
            continue;

        // Skip directives
        if (trimmed[0] == '@')
            continue;

        if (isBareC4ID(trimmed)) {
            c4::ID id = c4::ID::Parse(trimmed);

            if (first_line && current.entries.empty()) {
                // First bare ID: external base reference
                current.baseID = id;
            } else if (current.entries.empty() && !sections.empty()) {
                // Consecutive bare IDs: update base
                current.baseID = id;
            } else {
                // Flush current section
                if (!current.entries.empty())
                    sections.push_back(std::move(current));
                current = PatchSection();
                current.baseID = id;
            }
            first_line = false;
            continue;
        }

        // Auto-detect indent width
        if (indent_width < 0) {
            size_t indent = 0;
            while (indent < line.size() && line[indent] == ' ')
                indent++;
            if (indent > 0)
                indent_width = static_cast<int>(indent);
        }

        Entry entry = parseEntryLine(line, indent_width > 0 ? indent_width : 2);
        current.entries.push_back(std::move(entry));
        first_line = false;
    }

    // Flush final section (only if it has entries)
    if (!current.entries.empty())
        sections.push_back(std::move(current));

    return sections;
}

std::vector<PatchSection> DecodePatchChain(std::string_view data) {
    std::istringstream ss{std::string(data)};
    return DecodePatchChain(ss);
}

// -----------------------------------------------------------------------
// ResolvePatchChain
// -----------------------------------------------------------------------

Manifest ResolvePatchChain(const std::vector<PatchSection> &sections, int stopAt) {
    if (sections.empty()) {
        Manifest m;
        return m;
    }

    int limit = static_cast<int>(sections.size());
    if (stopAt > 0 && stopAt < limit)
        limit = stopAt;

    // First section is the base
    Manifest m;
    for (const auto &e : sections[0].entries)
        m.AddEntry(e);

    // Apply subsequent patches
    for (int i = 1; i < limit; i++) {
        Manifest patch;
        for (const auto &e : sections[static_cast<size_t>(i)].entries)
            patch.AddEntry(e);
        m = ApplyPatch(m, patch);
    }

    return m;
}

// -----------------------------------------------------------------------
// Merge (three-way)
// -----------------------------------------------------------------------

// conflictName appends ".conflict" to a path, preserving directory trailing slash.
static std::string conflictName(const std::string &path) {
    if (!path.empty() && path.back() == '/') {
        return path.substr(0, path.size() - 1) + ".conflict/";
    }
    return path + ".conflict";
}

// pathToDepth returns depth from a full path.
static int pathToDepth(const std::string &fullPath) {
    std::string clean = fullPath;
    if (!clean.empty() && clean.back() == '/')
        clean.pop_back();
    int count = 0;
    for (char c : clean) {
        if (c == '/') count++;
    }
    return count;
}

// pathEntryName extracts the last component of a full path.
static std::string pathEntryName(const std::string &fullPath) {
    bool is_dir = !fullPath.empty() && fullPath.back() == '/';
    std::string clean = fullPath;
    if (is_dir)
        clean.pop_back();
    auto idx = clean.rfind('/');
    std::string name = clean;
    if (idx != std::string::npos)
        name = clean.substr(idx + 1);
    if (is_dir)
        name += '/';
    return name;
}

// ensureDirs creates directory entries for any implied parent paths.
static void ensureDirs(std::map<std::string, Entry> &m) {
    std::vector<std::string> dirs;
    for (const auto &kv : m) {
        const std::string &p = kv.first;
        std::string clean = p;
        if (!clean.empty() && clean.back() == '/')
            clean.pop_back();

        // Split by /
        size_t pos = 0;
        while (true) {
            pos = clean.find('/', pos);
            if (pos == std::string::npos)
                break;
            std::string dir_path = clean.substr(0, pos + 1);
            if (m.find(dir_path) == m.end())
                dirs.push_back(dir_path);
            pos++;
        }
    }

    for (const auto &d : dirs) {
        if (m.find(d) != m.end())
            continue;
        Entry e;
        e.mode = ModeDir | 0755;
        e.size = NullSize;
        e.name = pathEntryName(d);
        e.depth = pathToDepth(d);
        m[d] = e;
    }
}

// addConflict adds both versions and records the conflict.
static void addConflict(std::map<std::string, Entry> &merged,
                        std::vector<Conflict> &conflicts,
                        const std::string &path,
                        const Entry *local, const Entry *remote)
{
    Conflict c;
    c.path = path;
    c.localDeleted = (local == nullptr);
    c.remoteDeleted = (remote == nullptr);
    if (local) c.localEntry = *local;
    if (remote) c.remoteEntry = *remote;
    conflicts.push_back(c);

    if (!local) {
        merged[path] = *remote;
        return;
    }
    if (!remote) {
        merged[path] = *local;
        return;
    }

    // LWW: newer timestamp wins
    const Entry *winner = local;
    const Entry *loser = remote;
    if (remote->timestamp > local->timestamp) {
        winner = remote;
        loser = local;
    }

    merged[path] = *winner;

    // Preserve the losing version with .conflict suffix
    std::string conflict_path = conflictName(path);
    merged[conflict_path] = *loser;
}

MergeResult Merge(const Manifest &base, const Manifest &local, const Manifest &remote) {
    auto base_map = EntryPaths(base.Entries());
    auto local_map = EntryPaths(local.Entries());
    auto remote_map = EntryPaths(remote.Entries());

    // Collect all unique paths
    std::set<std::string> all_set;
    for (const auto &kv : base_map) all_set.insert(kv.first);
    for (const auto &kv : local_map) all_set.insert(kv.first);
    for (const auto &kv : remote_map) all_set.insert(kv.first);

    std::map<std::string, Entry> merged;
    std::vector<Conflict> conflicts;

    for (const auto &p : all_set) {
        auto b_it = base_map.find(p);
        auto l_it = local_map.find(p);
        auto r_it = remote_map.find(p);
        const Entry *b = (b_it != base_map.end()) ? b_it->second : nullptr;
        const Entry *l = (l_it != local_map.end()) ? l_it->second : nullptr;
        const Entry *r = (r_it != remote_map.end()) ? r_it->second : nullptr;

        // Only one side has it
        if (!b && l && !r) {
            merged[p] = *l;
            continue;
        }
        if (!b && !l && r) {
            merged[p] = *r;
            continue;
        }

        // Both added
        if (!b && l && r) {
            if (mergeEqual(*l, *r)) {
                merged[p] = *l;
            } else {
                addConflict(merged, conflicts, p, l, r);
            }
            continue;
        }

        // All three exist
        if (b && l && r) {
            bool l_changed = !mergeEqual(*b, *l);
            bool r_changed = !mergeEqual(*b, *r);
            if (!l_changed && !r_changed) {
                merged[p] = *b;
            } else if (l_changed && !r_changed) {
                merged[p] = *l;
            } else if (!l_changed && r_changed) {
                merged[p] = *r;
            } else {
                if (mergeEqual(*l, *r)) {
                    merged[p] = *l; // converged
                } else {
                    addConflict(merged, conflicts, p, l, r);
                }
            }
            continue;
        }

        // Remote deleted
        if (b && l && !r) {
            if (mergeEqual(*b, *l)) {
                // Unchanged locally, remote deleted -> delete
            } else {
                addConflict(merged, conflicts, p, l, nullptr);
            }
            continue;
        }

        // Local deleted
        if (b && !l && r) {
            if (mergeEqual(*b, *r)) {
                // Unchanged remotely, local deleted -> delete
            } else {
                addConflict(merged, conflicts, p, nullptr, r);
            }
            continue;
        }

        // Both deleted
        if (b && !l && !r) {
            // Agreement: both deleted
            continue;
        }
    }

    // Ensure parent directories exist
    ensureDirs(merged);

    // Rebuild manifest from merged map
    MergeResult result;
    std::vector<std::string> paths;
    for (const auto &kv : merged)
        paths.push_back(kv.first);
    std::sort(paths.begin(), paths.end());

    for (const auto &p : paths) {
        Entry e = merged[p];
        e.name = pathEntryName(p);
        e.depth = pathToDepth(p);
        result.merged.AddEntry(std::move(e));
    }

    result.conflicts = std::move(conflicts);
    return result;
}

} // namespace c4m
