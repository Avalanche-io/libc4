// SPDX-License-Identifier: Apache-2.0
// C4M format parser -- streaming line-by-line decoder.
// Matches Go reference: github.com/Avalanche-io/c4/c4m/decoder.go
// Format is entry-only: no header, no directives. Lines starting with @ are rejected.

#include "c4/c4m.hpp"
#include "entry_line.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

// Read a line from stream. Rejects CR (0x0D) per spec.
bool readLine(std::istream &in, std::string &line, int &line_num) {
    if (!std::getline(in, line))
        return false;
    line_num++;

    // Reject CR: c4m requires LF-only line endings.
    for (char c : line) {
        if (c == '\r') {
            throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                     ": CR (0x0D) not allowed -- c4m requires LF-only line endings");
        }
    }
    return true;
}

// Check if a string is a bare C4 ID (exactly 90 chars, starts with "c4").
bool isBareC4ID(const std::string &s) {
    return s.size() == 90 && s[0] == 'c' && s[1] == '4';
}

// c4IDPattern for validation of each 90-char chunk.
static const std::regex &c4IDPatternRe() {
    static const std::regex re("^c4[1-9A-HJ-NP-Za-km-z]{88}$");
    return re;
}

// Check if a line is an inline ID list (len > 90, multiple of 90, all valid C4 IDs).
bool isInlineIDList(const std::string &s) {
    size_t n = s.size();
    if (n <= 90 || n % 90 != 0 || s[0] != 'c' || s[1] != '4')
        return false;
    for (size_t i = 0; i < n; i += 90) {
        if (!std::regex_match(s.begin() + static_cast<long>(i),
                              s.begin() + static_cast<long>(i + 90),
                              c4IDPatternRe()))
            return false;
    }
    return true;
}

// Parse a backslash-escaped name or target. Field-boundary escapes only:
// \<space> -> space, \" -> ", \[ -> [, \] -> ]
// All other backslash sequences pass through for UnsafeName.
// Returns the parsed string and advances pos.
// If is_name is true, '/' terminates the name (inclusive, for directories).
// Boundary detection: space followed by ->, <-, <>, c4 prefix, or -.
struct NameResult {
    std::string value;
    std::string raw;
    bool has_unescaped_brackets = false;
};

NameResult parseNameOrTarget(const std::string &line, size_t &pos, bool is_name) {
    size_t n = line.size();
    NameResult result;
    size_t raw_start = pos;

    while (pos < n) {
        char ch = line[pos];

        // A backslash always starts a two-byte escape. A c4m field-boundary
        // escape (\<space>, \", \[, \]) becomes its literal; any other (a
        // SafeName escape such as \\) passes through whole for UnsafeName,
        // so its second byte never starts an escape of its own. A name
        // ending in a backslash read back with its C4 ID swallowed before
        // this (backslash-name vector).
        if (ch == '\\' && pos + 1 < n) {
            char next = line[pos + 1];
            if (next == ' ' || next == '"' || next == '[' || next == ']') {
                result.value += next;
            } else {
                result.value += ch;
                result.value += next;
            }
            pos += 2;
            continue;
        }

        if (ch == '[' || ch == ']') {
            result.has_unescaped_brackets = true;
        }

        // A double quote is written \" in a name, and a control byte is
        // encoded by SafeName, so a raw one never comes from a conforming
        // writer: it is damage, refused rather than read into the name
        // (name-escape vector). parseEntryFromLine names the line.
        if (is_name && ch == '"')
            throw std::invalid_argument("name holds a raw '\"' (written \\\" in a name)");
        if (is_name && static_cast<unsigned char>(ch) < 0x20) {
            char hex[8];
            std::snprintf(hex, sizeof hex, "0x%02X", static_cast<unsigned char>(ch));
            throw std::invalid_argument(std::string("name holds a raw control byte ") + hex + " (SafeName encodes it)");
        }

        // A "/" ends a directory's name, and only where the field ends with
        // it. Anything after it but a space is part of the name, which then
        // holds a "/" and is refused with its line once decoded. Before, the
        // name was cut at its first "/" and the rest of the line, C4 ID
        // included, dropped (name-rule vector).
        if (is_name && ch == '/' && (pos + 1 >= line.size() || line[pos + 1] == ' ')) {
            result.value += '/';
            pos++;
            result.raw = line.substr(raw_start, pos - raw_start);
            return result;
        }

        // Boundary: space(s) followed by link operator, c4 prefix, or null
        // marker. Aligned (pretty) forms pad fields with runs of spaces,
        // so the lookahead skips the whole run before testing; the name
        // ends at the first space of the run.
        if (ch == ' ') {
            size_t j = pos;
            while (j < n && line[j] == ' ')
                j++;
            std::string_view rest(line.data() + j, n - j);
            if (rest.empty()) {
                // Trailing padding: the name ended at the first space.
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            if (rest.substr(0, 3) == "-> " ||
                rest.substr(0, 3) == "<- " ||
                rest.substr(0, 3) == "<> ")
            {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            // Hard link group: "->N" where N is digit 1-9
            if (rest.size() >= 3 && rest[0] == '-' && rest[1] == '>' &&
                rest[2] >= '1' && rest[2] <= '9')
            {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            if (rest.size() >= 2 && rest[0] == 'c' && rest[1] == '4') {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            if (rest[0] == '-' && (rest.size() == 1 || rest[1] == ' ')) {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            // A space that is not a field boundary would be part of the
            // name, but a name's space is written "\ ": an unescaped one is
            // damage, often a damaged C4 ID field that would otherwise join
            // the name.
            if (is_name)
                throw std::invalid_argument("name '" + result.value + " " +
                                            std::string(rest.substr(0, rest.find(' '))) +
                                            "' holds an unescaped space (written \"\\ \" in a name), "
                                            "or its C4 ID field is damaged");
        }

        result.value += ch;
        pos++;
    }

    result.raw = line.substr(raw_start, pos - raw_start);
    return result;
}

// startsIDField reports whether the run of spaces at pos is followed by
// the C4 ID field: a C4 ID, or the null "-" as the last field. The pretty
// form pads before that field, so the whole run is skipped first.
bool startsIDField(const std::string &line, size_t pos) {
    while (pos < line.size() && line[pos] == ' ')
        pos++;
    std::string_view rest(line.data() + pos, line.size() - pos);
    return rest.substr(0, 2) == "c4" || rest == "-" || rest.substr(0, 2) == "- ";
}

// isLoneIDField reports whether s, the rest of a line after a link
// operator, is exactly one field that is a C4 ID or the null "-". A field
// ends at an unescaped space.
bool isLoneIDField(const std::string &s) {
    if (s == "-")
        return true;
    if (s.size() < 2 || s[0] != 'c' || s[1] != '4')
        return false;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\') {
            i++;
            continue;
        }
        if (s[i] == ' ')
            return false;
    }
    try {
        c4::ID::Parse(s);
    } catch (const std::invalid_argument &) {
        return false;
    }
    return true;
}

// Parse symlink target. Unlike name, '/' is NOT a boundary.
std::string parseSymlinkTarget(const std::string &line, size_t &pos) {
    size_t n = line.size();
    std::string buf;

    while (pos < n) {
        char ch = line[pos];

        // A backslash always starts a two-byte escape (as in names): space
        // and quote become literals, any other passes through whole.
        if (ch == '\\' && pos + 1 < n) {
            char next = line[pos + 1];
            if (next == ' ' || next == '"') {
                buf += next;
            } else {
                buf += ch;
                buf += next;
            }
            pos += 2;
            continue;
        }

        // The pretty form pads before the C4 ID field; testing only the
        // next character let that padding join the target (link-target
        // vector).
        if (ch == ' ' && startsIDField(line, pos))
            return buf;

        buf += ch;
        pos++;
    }

    return buf;
}

// Check if text at position looks like a flow target (label: pattern).
bool isFlowTarget(const std::string &s, size_t pos) {
    if (pos >= s.size()) return false;
    char ch = s[pos];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')))
        return false;
    for (size_t i = pos + 1; i < s.size(); i++) {
        char c = s[i];
        if (c == ':') return true;
        if (c == ' ') return false;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return false;
}

// Parse flow target: ends at space followed by c4 or -, or end of line.
std::string parseFlowTarget(const std::string &line, size_t &pos) {
    size_t n = line.size();
    size_t start = pos;
    while (pos < n) {
        if (line[pos] == ' ' && startsIDField(line, pos))
            return line.substr(start, pos - start);
        pos++;
    }
    return line.substr(start, pos - start);
}

// Skip spaces, return count.
size_t skipSpaces(const std::string &line, size_t &pos) {
    size_t start = pos;
    while (pos < line.size() && line[pos] == ' ')
        pos++;
    return pos - start;
}

// Sequence pattern regex for detecting [0001-0100] style ranges
static const std::regex &sequencePatternRe() {
    static const std::regex re("\\[([0-9,\\-:]+)\\]");
    return re;
}

// Check if raw text has unescaped sequence notation.
bool hasUnescapedSequenceNotation(const std::string &raw) {
    // Replace all escape sequences with neutral chars so escaped brackets don't match.
    std::string buf;
    buf.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            buf += '_';
            buf += '_';
            i++;
            continue;
        }
        buf += raw[i];
    }
    return std::regex_search(buf, sequencePatternRe());
}

} // anonymous namespace

namespace c4m {

// The message matches the Go reference's ErrTrailingID text and names the fix.
TrailingIDError::TrailingIDError(int line)
    : std::runtime_error("c4m: a bare C4 ID ends the file; a c4m file never ends with its own ID (line " +
                         std::to_string(line) +
                         "): delete that line, or rewrite the file with `c4 id FILE.c4m > fixed.c4m`"),
      line_(line) {}

Manifest Manifest::Parse(std::string_view data) {
    std::istringstream ss{std::string(data)};
    return Parse(ss);
}

Manifest Manifest::ParseFile(const std::filesystem::path &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("cannot open file: " + path.string());
    return Parse(f);
}

// parseEntryFromLine parses one manifest entry from a full (indentation-included)
// line. It detects and updates indent_width (auto-detected from the first
// indented line). Mirrors the Go reference decoder.parseEntryFromLine.
static Entry parseEntryFields(const std::string &line, int &indent_width, int line_num) {
    // Detect indentation
    size_t indent = 0;
    while (indent < line.size() && line[indent] == ' ')
        indent++;

    if (indent_width < 0 && indent > 0)
        indent_width = static_cast<int>(indent);

    int depth = 0;
    if (indent_width > 0 && indent > 0)
        depth = static_cast<int>(indent) / indent_width;

    // Content after indentation
    std::string content = line.substr(indent);
    if (content.empty())
        throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                 ": empty entry");

    // Parse mode
    size_t pos = 0;
    std::string mode_str;

    if (content.size() >= 2 && content[0] == '-' && content[1] == ' ') {
        mode_str = "-";
        pos = 2;
    } else if (content.size() >= 11) {
        mode_str = content.substr(0, 10);
        pos = 11;
    } else {
        throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                 ": line too short");
    }

    // "-" is null (0); every ten-character mode is known, "----------"
    // included (ZeroMode).
    uint32_t mode = 0;
    if (mode_str != "-")
        mode = ParseMode(mode_str);

    Entry entry;
    entry.mode = mode;
    entry.depth = depth;

    // Parse timestamp
    if (pos >= content.size())
        throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                 ": missing timestamp");

    std::string ts_str;
    if (content[pos] == '-' && (pos + 1 >= content.size() || content[pos + 1] == ' ')) {
        ts_str = "-";
        pos += 2;
    } else if (content[pos] == '0' && (pos + 1 >= content.size() || content[pos + 1] == ' ')) {
        ts_str = "0";
        pos += 2;
    } else {
        if (content.size() >= pos + 20 && content[pos + 4] == '-' && content[pos + 10] == 'T') {
            size_t end_idx = pos + 20;
            if (content.size() >= pos + 25 &&
                (content[pos + 19] == '+' || content[pos + 19] == '-'))
                end_idx = pos + 25;
            ts_str = content.substr(pos, end_idx - pos);
            pos = end_idx;
            if (pos < content.size() && content[pos] == ' ')
                pos++;
        } else {
            // Pretty format: five space-separated fields, e.g.
            // "Jan  2 03:04:05 2026 UTC" (Go's pretty encoder). The
            // fields are taken from the line as written; the rest of the
            // line is parsed in place, so names keep their bytes.
            size_t p = pos;
            int fields = 0;
            while (fields < 5) {
                while (p < content.size() && content[p] == ' ')
                    p++;
                if (p >= content.size())
                    break;
                while (p < content.size() && content[p] != ' ')
                    p++;
                fields++;
            }
            if (fields < 5)
                throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                         ": cannot parse timestamp");
            ts_str = content.substr(pos, p - pos);
            pos = p;
            if (pos < content.size() && content[pos] == ' ')
                pos++;
        }
    }
    entry.timestamp = ParseTimestamp(ts_str);

    // Parse size
    skipSpaces(content, pos);
    if (pos >= content.size())
        throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                 ": missing size");

    if (content[pos] == '-' && (pos + 1 >= content.size() || content[pos + 1] == ' ')) {
        entry.size = NullSize;
        pos++;
    } else {
        size_t size_start = pos;
        while (pos < content.size() &&
               ((content[pos] >= '0' && content[pos] <= '9') || content[pos] == ','))
            pos++;
        if (pos == size_start)
            throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                     ": invalid size");
        std::string size_str;
        for (size_t i = size_start; i < pos; i++) {
            if (content[i] != ',')
                size_str += content[i];
        }
        entry.size = std::stoll(size_str);
    }
    // The size token ends at a space: "6x a.txt" read as size 6 and the
    // name "x a.txt", a different entry with no error (damaged-line
    // vector).
    if (pos < content.size() && content[pos] != ' ')
        throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                 ": invalid size");

    // Skip space after size
    skipSpaces(content, pos);

    // Parse name using backslash-escaped name parser
    if (pos >= content.size())
        throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                 ": missing name");

    auto name_result = parseNameOrTarget(content, pos, true);
    entry.name = UnsafeName(name_result.value);
    // A name is a bare entry name, checked once decoded, so an escaped "/"
    // is refused as surely as a raw one (name-rule vector).
    if (detail::isPathName(entry.name))
        throw std::runtime_error("c4m: line " + std::to_string(line_num) + ": name '" + entry.name +
                                 "' is a path, not an entry name (only a directory's trailing / is allowed)");

    // Ten dashes on a directory can only be a legacy null: a correct
    // encoder writes a zero-permission directory as "d---------", and
    // earlier pretty encoders wrote null as ten dashes.
    if (mode_str == "----------" && !entry.name.empty() && entry.name.back() == '/') {
        mode = 0;
        entry.mode = 0;
    }

    // Check for sequence notation in raw name
    if (hasUnescapedSequenceNotation(name_result.raw)) {
        entry.is_sequence = true;
        entry.pattern = entry.name;
    }

    // Skip whitespace
    skipSpaces(content, pos);

    // Parse link operators: ->, <-, <>
    bool is_symlink = (mode & ModeSymlink) != 0;

    if (pos + 1 < content.size() && content[pos] == '-' && content[pos + 1] == '>') {
        pos += 2;

        if (is_symlink) {
            // Symlink mode: -> is always a symlink target
            skipSpaces(content, pos);
            if (pos < content.size()) {
                entry.target = UnsafeName(parseSymlinkTarget(content, pos));
                skipSpaces(content, pos);
            }
        } else if (pos < content.size() && content[pos] >= '1' && content[pos] <= '9') {
            // Hard link group number: ->N
            size_t group_start = pos;
            while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9')
                pos++;
            entry.hard_link = std::stoi(content.substr(group_start, pos - group_start));
            skipSpaces(content, pos);
        } else {
            skipSpaces(content, pos);

            // Determine type by examining token after ->
            if (pos < content.size() && isFlowTarget(content, pos)) {
                entry.flow_direction = FlowDirection::Outbound;
                entry.flow_target = parseFlowTarget(content, pos);
                skipSpaces(content, pos);
            } else {
                std::string remaining;
                if (pos < content.size()) {
                    remaining = content.substr(pos);
                    while (!remaining.empty() && remaining.back() == ' ')
                        remaining.pop_back();
                }
                if (isLoneIDField(remaining)) {
                    // Hard link (ungrouped): the C4 ID field (or its null)
                    // is the only field after the operator. The C4 ID is
                    // always the last field and a target never holds an
                    // unescaped space, so a second field means the first
                    // is a target. A target that merely starts with "c4",
                    // under a mode that does not say symlink (a null mode,
                    // as views carry links), was read as a hard link and
                    // refused (link-target vector).
                    entry.hard_link = -1;
                } else if (pos < content.size()) {
                    // Fallback: treat as symlink target
                    entry.target = UnsafeName(parseSymlinkTarget(content, pos));
                    skipSpaces(content, pos);
                }
            }
        }
    } else if (pos + 1 < content.size() && content[pos] == '<' && content[pos + 1] == '-') {
        pos += 2;
        skipSpaces(content, pos);
        entry.flow_direction = FlowDirection::Inbound;
        entry.flow_target = parseFlowTarget(content, pos);
        skipSpaces(content, pos);
    } else if (pos + 1 < content.size() && content[pos] == '<' && content[pos + 1] == '>') {
        pos += 2;
        skipSpaces(content, pos);
        entry.flow_direction = FlowDirection::Bidirectional;
        entry.flow_target = parseFlowTarget(content, pos);
        skipSpaces(content, pos);
    }

    // Parse C4 ID or null marker
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

    // A 0 beside the C4 ID of non-empty content is a null size written by
    // a surface that has no "-" (a mount); hold it as null so every consumer
    // sees one value for one meaning (ruled 2026-11-08; c4 #68).
    if (entry.SizeIsNull())
        entry.size = NullSize;

    return entry;
}

namespace detail {

// A damaged field is an error naming the line, never a value: the mode,
// timestamp, size and C4 ID parsers throw std::logic_error (invalid_argument,
// out_of_range) without one, so it is added here.
Entry parseEntryFromLine(const std::string &line, int &indent_width, int line_num) {
    try {
        return parseEntryFields(line, indent_width, line_num);
    } catch (const std::logic_error &e) {
        throw std::runtime_error("c4m: line " + std::to_string(line_num) + ": " + e.what());
    }
}

bool isPathName(const std::string &name) {
    if (name.empty())
        return true;
    std::string base = name;
    if (base.back() == '/')
        base.pop_back();
    if (base.empty() || base == "." || base == "..")
        return true;
    for (char c : base) {
        if (c == '/' || c == '\0')
            return true;
    }
    return false;
}

} // namespace detail

// applyChainSection folds one accumulated section into the running manifest.
// The first section (before any checkpoint) is the base: its entries are
// appended directly. Every later section is a patch applied by name-and-path
// (add / modify / restate-identical with no children = removal / restate-
// identical directory with children = deepening, recursive). base_ is preserved
// across ApplyPatch so that, after an external base reference, checkpoint
// verification stays deferred to the resolver that fetches the base.
static void applyChainSection(Manifest &m, std::vector<Entry> &section, bool patch_mode) {
    if (!patch_mode) {
        for (auto &e : section)
            m.AddEntry(std::move(e));
    } else if (!section.empty()) {
        Manifest patch;
        for (auto &e : section)
            patch.AddEntry(std::move(e));
        c4::ID saved_base = m.Base();
        m = ApplyPatch(m, patch);
        m.SetBase(saved_base);
    }
    section.clear();
}

Manifest Manifest::Parse(std::istream &stream) {
    Manifest m;
    int line_num = 0;
    int indent_width = -1; // auto-detect

    // Chain resolution: entries accumulate in `section` between bare-C4-ID
    // boundaries. A bare ID resolves as follows (grammar erratum 2026-07-13,
    // matching Go reference decoder.go):
    //   - first non-blank line, no entries yet -> external base reference
    //   - otherwise a checkpoint naming the accumulated manifest state: flush
    //     the pending section (base append or patch apply), then verify the
    //     accumulated C4 ID against the checkpoint (reject on mismatch).
    // Consecutive checkpoints re-verify the same state and are accepted when
    // a patch follows. A bare ID at end of input is a format error
    // (TrailingIDError, ruled 2026-11-08): a c4m file never ends with its own
    // ID. Verification is skipped once an external base reference is present.
    std::vector<Entry> section;
    bool first_line = true;
    bool patch_mode = false;
    int last_checkpoint_line = 0; // line of the last bare ID if nothing followed it

    std::string line;
    while (readLine(stream, line, line_num)) {
        // Trim for classification checks
        std::string trimmed = line;
        size_t first_non_space = trimmed.find_first_not_of(' ');
        if (first_non_space == std::string::npos)
            trimmed.clear();
        else
            trimmed = trimmed.substr(first_non_space);

        // Skip blank lines (do not clear first_line: "first non-blank line").
        if (trimmed.empty())
            continue;

        // Skip inline ID lists (range data lines, not entries or boundaries).
        if (isInlineIDList(trimmed))
            continue;

        // Bare C4 ID line: patch boundary (base reference or checkpoint).
        if (isBareC4ID(trimmed)) {
            c4::ID id = c4::ID::Parse(trimmed);

            if (first_line && section.empty()) {
                // First line of stream: external base reference.
                m.SetBase(id);
            } else {
                last_checkpoint_line = line_num;
                applyChainSection(m, section, patch_mode);
                patch_mode = true;

                // A resolving decoder MUST verify checkpoints -- except after
                // an unresolved external base reference, where the accumulated
                // state is unknowable here.
                if (m.Base().IsNil()) {
                    c4::ID got = m.ComputeC4ID();
                    if (got != id) {
                        throw std::runtime_error(
                            "c4m: patch ID mismatch (line " + std::to_string(line_num) +
                            "): accumulated " + got.String() + ", checkpoint " + id.String());
                    }
                }
            }
            first_line = false;
            continue;
        }

        // Reject directive lines (lines starting with @).
        // Go reference behavior: directives are not supported.
        if (trimmed[0] == '@') {
            throw std::runtime_error("c4m: directives not supported (line " +
                                     std::to_string(line_num) + "): " + line);
        }

        section.push_back(detail::parseEntryFromLine(line, indent_width, line_num));
        first_line = false;
        last_checkpoint_line = 0;
    }

    // A bare C4 ID at end of input is a format error. The checkpoint was
    // verified above, so a wrong one has already thrown a mismatch; a right
    // one is refused here, naming the fix. A file that is only a first-line
    // base reference is not a trailing ID.
    if (last_checkpoint_line != 0)
        throw TrailingIDError(last_checkpoint_line);

    // Flush the final section: a chain ends on its last section's entries,
    // and the final patch is verified by what it names, not by a line after
    // it (C4M-STANDARD 10.7).
    applyChainSection(m, section, patch_mode);

    return m;
}

} // namespace c4m
