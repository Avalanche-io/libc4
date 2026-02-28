// SPDX-License-Identifier: Apache-2.0
// C4M format parser — streaming line-by-line decoder.
// Matches Go reference: github.com/Avalanche-io/c4/c4m/decoder.go

#include "c4/c4m.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

// Read a line from stream, stripping \r\n or \n.
bool readLine(std::istream &in, std::string &line) {
    if (!std::getline(in, line))
        return false;
    // Strip trailing \r (for CRLF)
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return true;
}

// Parse a quoted name/target: "..." with backslash escapes.
// Returns the parsed string and advances pos past the closing quote.
std::string parseQuoted(const std::string &line, size_t &pos) {
    pos++; // skip opening "
    std::string buf;
    size_t n = line.size();
    while (pos < n) {
        char ch = line[pos];
        if (ch == '\\' && pos + 1 < n) {
            char next = line[pos + 1];
            switch (next) {
            case '\\': buf += '\\'; break;
            case '"':  buf += '"'; break;
            case 'n':  buf += '\n'; break;
            default:   buf += '\\'; buf += next; break;
            }
            pos += 2;
        } else if (ch == '"') {
            pos++; // skip closing "
            return buf;
        } else {
            buf += ch;
            pos++;
        }
    }
    throw std::runtime_error("unterminated quoted name");
}

// Parse unquoted name. Terminates at:
// - '/' (inclusive, for directories)
// - space followed by "-> " or "c4" or "- " or end-of-line
std::string parseName(const std::string &line, size_t &pos) {
    size_t start = pos;
    size_t n = line.size();

    while (pos < n) {
        char ch = line[pos];

        // Directory: / terminates the name (inclusive)
        if (ch == '/') {
            pos++;
            return line.substr(start, pos - start);
        }

        // Check for boundary: space followed by boundary token
        if (ch == ' ' && pos + 1 < n) {
            // " -> "
            if (n - pos >= 4 && line.substr(pos, 4) == " -> ")
                return line.substr(start, pos - start);
            // " c4" (C4 ID)
            if (line[pos + 1] == 'c' && pos + 2 < n && line[pos + 2] == '4')
                return line.substr(start, pos - start);
            // " -" followed by end or space (null C4 ID)
            if (line[pos + 1] == '-' && (pos + 2 >= n || line[pos + 2] == ' '))
                return line.substr(start, pos - start);
        }
        pos++;
    }

    return line.substr(start);
}

// Parse symlink target. Unlike name, '/' is NOT a boundary.
// Terminates at space followed by "c4" prefix or "-" (null ID) or end-of-line.
std::string parseTarget(const std::string &line, size_t &pos) {
    if (pos < line.size() && line[pos] == '"')
        return parseQuoted(line, pos);

    size_t start = pos;
    size_t n = line.size();

    while (pos < n) {
        char ch = line[pos];
        if (ch == ' ' && pos + 1 < n) {
            if (line[pos + 1] == 'c' && pos + 2 < n && line[pos + 2] == '4')
                return line.substr(start, pos - start);
            if (line[pos + 1] == '-' && (pos + 2 >= n || line[pos + 2] == ' '))
                return line.substr(start, pos - start);
        }
        pos++;
    }

    return line.substr(start);
}

// Skip whitespace, return number of spaces skipped.
size_t skipSpaces(const std::string &line, size_t &pos) {
    size_t start = pos;
    while (pos < line.size() && line[pos] == ' ')
        pos++;
    return pos - start;
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

Manifest Manifest::Parse(std::istream &stream) {
    Manifest m;
    int line_num = 0;
    int indent_width = -1; // auto-detect

    // 1. Parse header
    std::string line;
    if (!readLine(stream, line))
        throw std::runtime_error("c4m: empty input");
    line_num++;

    if (line.rfind("@c4m ", 0) != 0)
        throw std::runtime_error("c4m: expected '@c4m X.Y' header, got: " + line);

    std::string version = line.substr(5);
    if (version.empty())
        throw std::runtime_error("c4m: missing version number");
    if (version[0] != '1')
        throw std::runtime_error("c4m: unsupported version: " + version);

    m.version_ = version;

    // 2. Parse entries and directives
    while (readLine(stream, line)) {
        line_num++;

        if (line.empty())
            continue;

        // Handle directives
        if (line[0] == '@' || (line.find_first_not_of(' ') != std::string::npos &&
                               line[line.find_first_not_of(' ')] == '@')) {
            std::string trimmed = line;
            // Trim leading spaces
            size_t first = trimmed.find_first_not_of(' ');
            if (first != std::string::npos)
                trimmed = trimmed.substr(first);

            // Parse known directives
            if (trimmed.rfind("@base ", 0) == 0) {
                std::string id_str = trimmed.substr(6);
                // Trim whitespace
                while (!id_str.empty() && id_str.back() == ' ')
                    id_str.pop_back();
                m.base_ = c4::ID::Parse(id_str);
            }
            // Skip other directives (@layer, @remove, @by, @time, @note, @data, @end)
            // for now — they are advanced features
            continue;
        }

        // Detect indentation
        size_t indent = 0;
        while (indent < line.size() && line[indent] == ' ')
            indent++;

        if (indent_width < 0 && indent > 0)
            indent_width = static_cast<int>(indent);

        int depth = 0;
        if (indent_width > 0 && indent > 0)
            depth = static_cast<int>(indent) / indent_width;

        // Trim leading spaces
        std::string content = line.substr(indent);
        if (content.empty())
            continue;

        // Parse mode
        size_t pos = 0;
        std::string mode_str;

        if (content.size() >= 2 && content[0] == '-' && content[1] == ' ') {
            // Single-dash null mode
            mode_str = "-";
            pos = 2;
        } else if (content.size() >= 11) {
            mode_str = content.substr(0, 10);
            pos = 11; // skip mode + space
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
            // Find end of timestamp (canonical: 20 chars for Z, 25 for offset)
            if (content.size() >= pos + 20 && content[pos + 4] == '-' && content[pos + 10] == 'T') {
                size_t end_idx = pos + 20; // assume Z
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
            // Strip commas
            std::string size_str;
            for (size_t i = size_start; i < pos; i++) {
                if (content[i] != ',')
                    size_str += content[i];
            }
            entry.size = std::stoll(size_str);
        }

        // Skip space after size
        skipSpaces(content, pos);

        // Parse name
        if (pos >= content.size())
            throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                     ": missing name");

        if (content[pos] == '"') {
            entry.name = parseQuoted(content, pos);
        } else {
            entry.name = parseName(content, pos);
        }

        // Skip whitespace
        skipSpaces(content, pos);

        // Check for symlink target " -> "
        if (pos + 2 < content.size() && content[pos] == '-' && content[pos + 1] == '>') {
            pos += 2;
            skipSpaces(content, pos);
            if (pos >= content.size())
                throw std::runtime_error("c4m: line " + std::to_string(line_num) +
                                         ": missing symlink target");
            entry.target = parseTarget(content, pos);
            skipSpaces(content, pos);
        }

        // Check for C4 ID
        if (pos < content.size()) {
            std::string remaining = content.substr(pos);
            // Trim trailing whitespace
            while (!remaining.empty() && remaining.back() == ' ')
                remaining.pop_back();

            if (remaining == "-") {
                // Null C4 ID — leave as nil
            } else if (remaining.size() >= 2 && remaining[0] == 'c' && remaining[1] == '4') {
                entry.id = c4::ID::Parse(remaining);
            }
        }

        m.entries_.push_back(std::move(entry));
    }

    return m;
}

} // namespace c4m
