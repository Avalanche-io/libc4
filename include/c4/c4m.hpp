// SPDX-License-Identifier: Apache-2.0
// C4M Format - C++ API
// Parser and encoder for .c4m files.
// Reference: Go implementation at github.com/Avalanche-io/c4/c4m

#ifndef C4_C4M_HPP
#define C4_C4M_HPP

#include "c4.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace c4m {

// Mode type bits (matching Go's os.FileMode layout for cross-language compat)
constexpr uint32_t ModeDir        = 0x80000000;
constexpr uint32_t ModeSymlink    = 0x08000000;
constexpr uint32_t ModeDevice     = 0x04000000;
constexpr uint32_t ModeNamedPipe  = 0x02000000;
constexpr uint32_t ModeSocket     = 0x01000000;
constexpr uint32_t ModeSetuid     = 0x00800000;
constexpr uint32_t ModeSetgid     = 0x00400000;
constexpr uint32_t ModeSticky     = 0x00200000;
constexpr uint32_t ModeCharDevice = 0x00100000;
constexpr uint32_t ModeTypeMask   = ModeDir | ModeSymlink | ModeDevice |
                                     ModeNamedPipe | ModeSocket | ModeCharDevice;
constexpr uint32_t ModePermMask   = 0777;

// Null sentinel: timestamp 0 (Unix epoch) means "unspecified"
constexpr int64_t NullTimestamp = 0;
// Null sentinel: size -1 means "unspecified"
constexpr int64_t NullSize = -1;

// Flow link direction
enum class FlowDirection {
    None = 0,        // No flow link
    Outbound,        // -> (content here propagates there)
    Inbound,         // <- (content there propagates here)
    Bidirectional,   // <> (bidirectional sync)
};

// A single entry in a .c4m manifest.
struct Entry {
    uint32_t mode = 0;         // Unix file mode (type + permission bits)
    int64_t timestamp = 0;     // Modification time (Unix seconds UTC), 0 = null
    int64_t size = 0;          // File size in bytes, -1 = null
    std::string name;          // File or directory name (dirs end with '/')
    std::string target;        // Symlink target (empty if not a symlink)
    c4::ID id;                 // Content identifier
    int depth = 0;             // Nesting depth (0 = root level)

    // Hard link marker: 0=none, -1=ungrouped (->), >0=group number (->N)
    int hard_link = 0;

    // Flow link: declares a cross-location data relationship.
    FlowDirection flow_direction = FlowDirection::None;
    std::string flow_target;   // Location reference (e.g., "nas:", "studio:plates/")

    // For sequences
    bool is_sequence = false;
    std::string pattern;       // Original sequence pattern

    bool IsDir() const;
    bool IsSymlink() const;
    bool HasNullValues() const;

    // Return the flow operator string ("->", "<-", "<>", or "")
    std::string FlowOperator() const;

    // Format as canonical c4m line (no indentation, no trailing newline).
    // Null mode renders as "-" (single dash).
    std::string Canonical() const;

    // Format with indentation (no trailing newline).
    // Null mode renders as "----------" in display format.
    std::string Format(int indent_width = 2) const;
};

// Tree index: lazily-built O(1) navigation structure for manifest entries.
struct TreeIndex {
    std::unordered_map<std::string, const Entry *> by_path;
    std::unordered_map<std::string, const Entry *> by_name;
    std::unordered_map<const Entry *, std::string> path_of;
    std::unordered_map<const Entry *, std::vector<const Entry *>> children;
    std::unordered_map<const Entry *, const Entry *> parent;
    std::vector<const Entry *> root;
};

// A parsed .c4m manifest.
class Manifest {
public:
    Manifest() = default;

    // Parse from string
    static Manifest Parse(std::string_view data);

    // Parse from file
    static Manifest ParseFile(const std::filesystem::path &path);

    // Parse from stream
    static Manifest Parse(std::istream &stream);

    // Encode to canonical c4m format (entry-only, no header)
    std::string Encode() const;

    // Write to file
    void WriteFile(const std::filesystem::path &path) const;

    // Sort entries (files before dirs at each level, natural sort within)
    void SortEntries();

    // Validate manifest structure (throws on error)
    void Validate() const;

    // Access fields
    const std::string &Version() const { return version_; }
    const c4::ID &Base() const { return base_; }
    const std::vector<Entry> &Entries() const { return entries_; }
    size_t EntryCount() const { return entries_.size(); }

    // Flat list of all entry pointers
    std::vector<const Entry *> AllEntries() const;

    // Lookup by full path (e.g., "src/main.go") via tree index (O(1)).
    const Entry *GetEntry(const std::string &path) const;

    // Lookup by bare entry name (e.g., "main.go"). Last one wins if ambiguous.
    const Entry *GetEntryByName(const std::string &name) const;

    // Full path of an entry within the manifest ("" if not found).
    std::string EntryPath(const Entry *e) const;

    // Direct children of a directory entry.
    std::vector<const Entry *> Children(const Entry *e) const;

    // Parent directory of an entry (nullptr for root-level entries).
    const Entry *Parent(const Entry *e) const;

    // Sibling entries (same parent, excluding the entry itself).
    std::vector<const Entry *> Siblings(const Entry *e) const;

    // Ancestors from immediate parent to root (inner-to-outer order).
    // For "a/b/c/deep.txt" returns [c/, b/, a/].
    std::vector<const Entry *> Ancestors(const Entry *e) const;

    // All entries nested under a directory, recursively.
    std::vector<const Entry *> Descendants(const Entry *e) const;

    // All depth-0 entries.
    std::vector<const Entry *> Root() const;

    // Entries at a specific depth.
    std::vector<const Entry *> GetEntriesAtDepth(int depth) const;

    // Sorted list of all full entry paths.
    std::vector<std::string> PathList() const;

    // Computed C4 ID of the canonical form.
    c4::ID RootID() const;

    // Computed C4 ID of the manifest (canonicalize + sort + hash root entries).
    c4::ID ComputeC4ID() const;

    // Mutators
    void AddEntry(Entry e);
    void SetVersion(const std::string &v) { version_ = v; }
    void SetBase(const c4::ID &id) { base_ = id; }

    // Remove entry (and descendants if directory) from the manifest.
    void RemoveEntry(const Entry *e);

    // Move entry to a new parent with a new name. If new_parent is nullptr
    // the entry moves to root (depth 0). Updates descendant depths.
    void MoveEntry(const Entry *e, const Entry *new_parent,
                   const std::string &new_name);

    // Deep copy of the manifest.
    Manifest Copy() const;

    // Propagate metadata from children to parents (sizes, timestamps).
    void Canonicalize();

    // Force the tree index to be rebuilt on next access.
    void InvalidateIndex();

    // Glob-match filter returning a new manifest (matches bare entry names).
    Manifest FilterByPath(const std::string &pattern) const;

    // Prefix filter returning a new manifest (matches full paths).
    Manifest FilterByPrefix(const std::string &prefix) const;

    // Scan directory (not yet implemented)
    static Manifest Scan(const std::filesystem::path &dir);

    // Legacy set operations (not yet implemented)
    Manifest Diff(const Manifest &other) const;
    Manifest Merge(const Manifest &other) const;
    Manifest Intersect(const Manifest &other) const;

private:
    std::string version_ = "1.0";
    std::vector<Entry> entries_;
    c4::ID base_;
    mutable std::unique_ptr<TreeIndex> index_;

    const TreeIndex &ensureIndex() const;
    void invalidateIndex();
};

// -----------------------------------------------------------------------
// Operation result types
// -----------------------------------------------------------------------

// PatchResult: output of PatchDiff — the delta between two manifests.
struct PatchResult {
    Manifest patch;  // Entries constituting the patch delta
    c4::ID oldID;    // C4 ID of the old state
    c4::ID newID;    // C4 ID of the new state

    bool IsEmpty() const { return patch.EntryCount() == 0; }
};

// DiffResult: categorized comparison of two manifests.
struct DiffResult {
    Manifest added;
    Manifest removed;
    Manifest modified;
    Manifest same;

    bool IsEmpty() const {
        return added.EntryCount() == 0 &&
               removed.EntryCount() == 0 &&
               modified.EntryCount() == 0;
    }
};

// PatchSection: one section of a patch chain (base or delta).
struct PatchSection {
    c4::ID baseID;               // C4 ID preceding this section (nil for first)
    std::vector<Entry> entries;   // Entries in this section
};

// Conflict: a merge conflict where both sides changed the same path.
struct Conflict {
    std::string path;            // Full path (e.g., "footage/shot01.mov")
    Entry localEntry;            // Zero-mode sentinel if side deleted
    Entry remoteEntry;           // Zero-mode sentinel if side deleted
    bool localDeleted = false;   // True if local side deleted
    bool remoteDeleted = false;  // True if remote side deleted
};

// -----------------------------------------------------------------------
// Operations — free functions matching Go reference
// -----------------------------------------------------------------------

// PatchDiff compares old and new manifests, producing a patch delta.
PatchResult PatchDiff(const Manifest &old_m, const Manifest &new_m);

// ApplyPatch applies patch entries to a base manifest.
Manifest ApplyPatch(const Manifest &base, const Manifest &patch);

// Diff compares two manifests and returns categorized results.
DiffResult Diff(const Manifest &a, const Manifest &b);

// EntryPaths builds a map from full path to entry pointer.
std::map<std::string, const Entry *> EntryPaths(const std::vector<Entry> &entries);

// EncodePatch encodes a PatchResult as c4m text (oldID + entries + newID).
std::string EncodePatch(const PatchResult &pr);

// DecodePatchChain parses c4m text into sections separated by bare C4 ID lines.
std::vector<PatchSection> DecodePatchChain(std::istream &input);
std::vector<PatchSection> DecodePatchChain(std::string_view data);

// ResolvePatchChain applies patches sequentially. stopAt=0 means all.
Manifest ResolvePatchChain(const std::vector<PatchSection> &sections, int stopAt = 0);

// Merge performs a three-way merge. Returns merged manifest and conflicts.
struct MergeResult {
    Manifest merged;
    std::vector<Conflict> conflicts;
};
MergeResult Merge(const Manifest &base, const Manifest &local, const Manifest &remote);

// -----------------------------------------------------------------------
// Utilities
// -----------------------------------------------------------------------

// Natural sort: numeric segments compared as integers.
// "file2.txt" < "file10.txt"
bool NaturalLess(const std::string &a, const std::string &b);

// Mode string conversion
std::string FormatMode(uint32_t mode);
uint32_t ParseMode(const std::string &s);

// Timestamp conversion (RFC3339 UTC)
std::string FormatTimestamp(int64_t unix_seconds);
int64_t ParseTimestamp(const std::string &s);

// SafeName encoding (Universal Filename Encoding, three-tier system).
// Tier 1: printable UTF-8 passthrough (except currency sign U+00A4 and backslash)
// Tier 2: backslash escapes: \0, \t, \n, \r, \\
// Tier 3: non-printable bytes as braille U+2800-U+28FF between currency sign delimiters
std::string SafeName(const std::string &raw);

// UnsafeName reverses SafeName encoding.
std::string UnsafeName(const std::string &encoded);

// EscapeField applies c4m field-boundary escaping on top of SafeName:
// space -> "\ ", double-quote -> "\"", and for non-sequences, [ -> "\[", ] -> "\]".
std::string EscapeField(const std::string &name, bool is_sequence);

// UnescapeField reverses EscapeField (field-boundary unescaping only).
std::string UnescapeField(const std::string &escaped);

// -----------------------------------------------------------------------
// c4m Detection
// -----------------------------------------------------------------------

// LooksLikeC4m performs the cheap Phase 1 heuristic: skip leading spaces,
// check if the first non-space byte is a valid c4m mode character
// ('-', 'd', 'l', 'p', 's', 'b', 'c'). Returns true if content might be
// c4m and deserves a full parse attempt.
bool LooksLikeC4m(std::string_view data);

// CanonicalizeC4m attempts to parse data as c4m. On success, returns the
// canonical encoding (sorted, deterministic). On failure, returns empty.
std::string CanonicalizeC4m(std::string_view data);

} // namespace c4m

#endif // C4_C4M_HPP
