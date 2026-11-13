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
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace c4m {

// TrailingIDError is thrown when a stream ends with a bare C4 ID (Go's
// c4m.ErrTrailingID). A c4m file never ends with its own ID: a bare ID is
// the first-line base reference or a checkpoint before a patch, and a file
// that ends with one is a format error (ruled 2026-11-08). Chains grown
// with `c4 diff >>` by c4 1.0.13 through 1.0.26 have this shape; the c4
// normaliser (`c4 id FILE.c4m`, c4 1.1) rewrites them. line() is the line
// of the trailing ID. It derives from std::runtime_error, so existing
// handlers still catch it.
class TrailingIDError : public std::runtime_error {
public:
    explicit TrailingIDError(int line);
    int line() const noexcept { return line_; }

private:
    int line_;
};

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

// ZeroMode is the known zero mode: a regular file with no permission
// bits, written "----------". It is not the null mode, which is 0 and is
// written "-". The two differ for progressive resolution: null means not
// yet checked, zero means checked and found zero. Bit 18 is set by no
// operating system mode and is outside ModeTypeMask and ModePermMask, so
// ZeroMode acts as mode 0 everywhere a mode is applied. (Mirrors Go
// c4m.ZeroMode.)
constexpr uint32_t ZeroMode = 1u << 18;

// KnownMode returns m as a known mode: a mode read from disk or decoded
// from a ten-character field is never null, so an all-zero mode becomes
// ZeroMode.
constexpr uint32_t KnownMode(uint32_t m) { return m == 0 ? ZeroMode : m; }

// IsNullMode reports whether m is the null (unspecified) mode.
constexpr bool IsNullMode(uint32_t m) { return m == 0; }

// Null sentinel: timestamp 0 (Unix epoch) means "unspecified"
constexpr int64_t NullTimestamp = 0;
// Null sentinel: size -1 means "unspecified"
constexpr int64_t NullSize = -1;

// EmptyContentID is the C4 ID of zero bytes: the only C4 ID a size of 0 can
// truthfully sit beside, and the ID of an empty directory's listing.
// (Mirrors Go c4m.EmptyContentID; a function so it is safe to use during
// static initialisation.)
const c4::ID &EmptyContentID();

// Flow link direction
enum class FlowDirection {
    None = 0,        // No flow link
    Outbound,        // -> (content here propagates there)
    Inbound,         // <- (content there propagates here)
    Bidirectional,   // <> (bidirectional sync)
};

// A single entry in a .c4m manifest.
struct Entry {
    uint32_t mode = 0;         // Unix file mode (type + permission bits); 0 = null, ZeroMode = known zero
    int64_t timestamp = 0;     // Modification time (Unix seconds UTC), 0 = null
    int64_t size = 0;          // File size in bytes, -1 = null (see SizeIsNull)
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

    // SizeIsNull reports whether the size is unknown: -1, or a stated 0
    // beside a C4 ID other than EmptyContentID() on an entry that is not a
    // symlink. A filesystem surface has no "-", so a null size shows there
    // as 0, and only the empty content has size 0 (ruled 2026-11-08;
    // c4 #68). A 0 with no C4 ID is a known zero (a structure-level scan of
    // an empty file); a symlink's size is the link's own and its C4 ID the
    // target's, so it is exempt. This is the one test every size check
    // asks.
    bool SizeIsNull() const;

    // Return the flow operator string ("->", "<-", "<>", or "")
    std::string FlowOperator() const;

    // Format as canonical c4m line (no indentation, no trailing newline).
    // Null mode renders as "-" (single dash); the known zero (ZeroMode)
    // as "----------".
    std::string Canonical() const;

    // Format with indentation (no trailing newline).
    // Null mode renders as "-" here too; the known zero as "----------".
    // Earlier encoders wrote null as ten dashes in display form, which
    // conflated the two (C4M-STANDARD §4.1).
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

    // Parse from string. A chain is resolved and its checkpoints verified;
    // a bare C4 ID at end of input throws TrailingIDError (a wrong one is a
    // checkpoint mismatch first). The same holds for ParseFile and the
    // stream form.
    static Manifest Parse(std::string_view data);

    // Parse from file
    static Manifest ParseFile(const std::filesystem::path &path);

    // Parse from stream
    static Manifest Parse(std::istream &stream);

    // Encode to canonical c4m format (no header). A set Base() is written
    // first, as the bare-ID line that names the external base; without one
    // the output is the entries only.
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

    // Inline range data: a folded sequence's ID list (the bare concatenation
    // of its members' C4 IDs, 90 characters each), keyed by the list's C4
    // ID, which is the sequence entry's C4 ID (SPECIFICATION.md, "Inline
    // Range Data"). Parse keeps the lines that trail the entries; Encode
    // writes them back after the entries, sorted by ID, as Go's encoder
    // does. They are not part of the manifest's identity.
    const std::map<c4::ID, std::string> &RangeData() const { return range_data_; }
    void SetRangeData(const c4::ID &id, const std::string &list) { range_data_[id] = list; }

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

private:
    std::string version_ = "1.0";
    std::vector<Entry> entries_;
    c4::ID base_;
    std::map<c4::ID, std::string> range_data_;
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

// ApplyPatch applies patch entries to a base manifest. Identical with no
// children under it = removal (entry and subtree); identical directory with
// children under it = deepening (entry kept, children applied recursively);
// otherwise replace; new path = addition.
//
// A patch is meaningful only over its own base. Because removal is spelled
// by exact restatement, applying a patch to any other tree reads an addition
// that meets an identical entry as a removal. To carry a patch's changes to
// another tree, read each entry against the patch's base first, then apply
// what it means; the rule and the shared conformance vectors are in c4 core
// (c4m/testdata/apply-from-vector). Not implemented here until a caller
// needs it.
Manifest ApplyPatch(const Manifest &base, const Manifest &patch);

// Diff compares two manifests and returns categorized results.
DiffResult Diff(const Manifest &a, const Manifest &b);

// EntryPaths builds a map from full path to entry pointer.
std::map<std::string, const Entry *> EntryPaths(const std::vector<Entry> &entries);

// EncodePatch encodes a PatchResult as c4m text: the old state's ID as the
// boundary, then the entries, nothing after. The new state's ID is never
// written: a c4m file never ends with its own ID (2026-11-08); the patch
// resolves to pr.newID.
std::string EncodePatch(const PatchResult &pr);

// DecodePatchChain parses c4m text into sections separated by bare C4 ID
// lines, without resolving or verifying them. A bare C4 ID at end of input
// throws TrailingIDError.
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

// Mode string conversion. ParseMode takes a ten-character mode and always
// returns a known mode ("----------" gives ZeroMode); the single-dash null
// is the decoder's business, not ParseMode's. FormatMode ignores the
// ZeroMode bit, so ZeroMode formats as "----------".
std::string FormatMode(uint32_t mode);
uint32_t ParseMode(const std::string &s);

// Timestamp conversion (RFC3339 UTC)
std::string FormatTimestamp(int64_t unix_seconds);
int64_t ParseTimestamp(const std::string &s);

// SafeName encoding (Universal Filename Encoding, three-tier system).
// Tier 1: printable UTF-8 passthrough (except currency sign U+00A4 and backslash).
//   Printable is Go's unicode.IsPrint pinned to Unicode 17.0.0 (letters,
//   marks, numbers, punctuation, symbols, and U+0020), read from an
//   embedded table, never from the platform, so a name encodes the same
//   way on every build.
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

} // namespace c4m

#endif // C4_C4M_HPP
