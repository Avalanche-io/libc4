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

        // Boundary: space followed by link operator, c4 prefix, or null marker
        if (ch == ' ') {
            std::string_view rest(line.data() + pos, n - pos);
            if (rest.substr(0, 4) == " -> " ||
                rest.substr(0, 4) == " <- " ||
                rest.substr(0, 4) == " <> ")
            {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            // Hard link group: " ->N" where N is digit 1-9
            if (rest.size() >= 4 && rest[1] == '-' && rest[2] == '>' &&
                rest[3] >= '1' && rest[3] <= '9')
            {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            if (rest.size() > 2 && rest[1] == 'c' && rest[2] == '4') {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
            if (rest.size() >= 2 && rest[1] == '-' && (rest.size() == 2 || rest[2] == ' ')) {
                result.raw = line.substr(raw_start, pos - raw_start);
                return result;
            }
        }

        result.value += ch;
        pos++;
    }

    result.raw = line.substr(raw_start, pos - raw_start);
    return result;
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

        if (ch == ' ') {
            std::string_view rest(line.data() + pos, n - pos);
            if (rest.size() > 2 && rest[1] == 'c' && rest[2] == '4') {
                return buf;
            }
            if (rest.size() >= 2 && rest[1] == '-' && (rest.size() == 2 || rest[2] == ' ')) {
                return buf;
            }
        }

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
        char ch = line[pos];
        if (ch == ' ') {
            std::string_view rest(line.data() + pos, n - pos);
            if (rest.size() > 2 && rest[1] == 'c' && rest[2] == '4') {
                return line.substr(start, pos - start);
            }
            if (rest.size() >= 2 && rest[1] == '-' && (rest.size() == 2 || rest[2] == ' ')) {
                return line.substr(start, pos - start);
            }
        }
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

// parseEntryFields parses one manifest entry from a full (indentation-
// included) line. It detects and updates indent_width (auto-detected from
// the first indented line).
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

    // This line of libc4 reads the canonical form only: one space between
    // fields. A run of unescaped spaces is the pretty form's column padding,
    // which it refuses, naming the line, rather than read: through 1.0.25 a
    // pretty line with a null timestamp parsed, and the padding became part
    // of the name and the link or flow target (unicode-name vector's
    // pretty.c4m). An escaped space ("\ ", part of a name) followed by the
    // field separator is not padding.
    for (size_t i = 0; i + 1 < content.size(); i++) {
        if (content[i] != ' ' || content[i + 1] != ' ')
            continue;
        size_t backslashes = 0;
        for (size_t j = i; j > 0 && content[j - 1] == '\\'; j--)
            backslashes++;
        if (backslashes % 2 == 1)
            continue; // content[i] is an escaped space; content[i+1] separates
        throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                 ": padded fields (the pretty form) are not read by this release line; "
                                 "a run of spaces between fields is refused");
    }
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

    uint32_t mode = 0;
    if (mode_str != "-" && mode_str != "----------") {
        mode = ParseMode(mode_str);
    }

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
            throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                     ": cannot parse timestamp");
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

Manifest Manifest::Parse(std::istream &stream) {
    Manifest m;
    int line_num = 0;
    int indent_width = -1; // auto-detect

    std::string line;
    while (readLine(stream, line, line_num)) {
        // Trim for classification checks
        std::string trimmed = line;
        size_t first_non_space = trimmed.find_first_not_of(' ');
        if (first_non_space == std::string::npos)
            trimmed.clear();
        else
            trimmed = trimmed.substr(first_non_space);

        // Skip blank lines
        if (trimmed.empty())
            continue;

        // Skip inline ID lists (range data lines, not entries)
        if (isInlineIDList(trimmed))
            continue;

        // Skip bare C4 ID lines (patch boundaries -- not yet supported in C++ impl)
        if (isBareC4ID(trimmed))
            continue;

        // Reject directive lines (lines starting with @).
        // Go reference behavior: directives are not supported.
        if (trimmed[0] == '@') {
            throw std::runtime_error("c4m: directives not supported (line " +
                                     std::to_string(line_num) + "): " + line);
        }

        m.entries_.push_back(detail::parseEntryFromLine(line, indent_width, line_num));
    }

    return m;
}

} // namespace c4m
