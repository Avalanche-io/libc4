// SPDX-License-Identifier: Apache-2.0
// C4M entry: mode/timestamp formatting, canonical form

#include "c4/c4m.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>

namespace c4m {

bool Entry::IsDir() const {
    return (mode & ModeDir) != 0 ||
           (!name.empty() && name.back() == '/');
}

bool Entry::IsSymlink() const {
    return (mode & ModeSymlink) != 0;
}

// Format name: quote if it contains spaces, quotes, backslashes, or newlines.
// Directory names (trailing /) are never quoted — the slash is an unambiguous boundary.
static std::string formatName(const std::string &name) {
    if (!name.empty() && name.back() == '/') {
        // Directories: escape backslashes and newlines but don't quote
        std::string out;
        out.reserve(name.size());
        for (char c : name) {
            if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else out += c;
        }
        return out;
    }

    bool needs_quotes = false;
    for (char c : name) {
        if (c == ' ' || c == '"' || c == '\\' || c == '\n') {
            needs_quotes = true;
            break;
        }
    }
    // Leading/trailing whitespace
    if (!name.empty() && (name.front() == ' ' || name.back() == ' '))
        needs_quotes = true;

    if (!needs_quotes)
        return name;

    std::string out;
    out.reserve(name.size() + 4);
    out += '"';
    for (char c : name) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    out += '"';
    return out;
}

static std::string formatTarget(const std::string &t) {
    bool needs_quotes = false;
    for (char c : t) {
        if (c == ' ' || c == '"' || c == '\\' || c == '\n') {
            needs_quotes = true;
            break;
        }
    }
    if (!t.empty() && (t.front() == ' ' || t.back() == ' '))
        needs_quotes = true;

    if (!needs_quotes)
        return t;

    std::string out;
    out.reserve(t.size() + 4);
    out += '"';
    for (char c : t) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    out += '"';
    return out;
}

std::string FormatMode(uint32_t mode) {
    char buf[11];

    // File type (first character)
    uint32_t type_bits = mode & ModeTypeMask;
    if (type_bits == 0)           buf[0] = '-'; // regular
    else if (type_bits & ModeDir)        buf[0] = 'd';
    else if (type_bits & ModeSymlink)    buf[0] = 'l';
    else if (type_bits & ModeNamedPipe)  buf[0] = 'p';
    else if (type_bits & ModeSocket)     buf[0] = 's';
    else if (type_bits & ModeDevice)     buf[0] = 'b';
    else if (type_bits & ModeCharDevice) buf[0] = 'c';
    else                                 buf[0] = '?';

    // Permission bits
    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) {
        if (mode & (1u << (8 - i)))
            buf[i + 1] = rwx[i];
        else
            buf[i + 1] = '-';
    }

    // Special bits
    if (mode & ModeSetuid) {
        buf[3] = (buf[3] == 'x') ? 's' : 'S';
    }
    if (mode & ModeSetgid) {
        buf[6] = (buf[6] == 'x') ? 's' : 'S';
    }
    if (mode & ModeSticky) {
        buf[9] = (buf[9] == 'x') ? 't' : 'T';
    }

    buf[10] = '\0';
    return buf;
}

uint32_t ParseMode(const std::string &s) {
    if (s.size() != 10)
        throw std::invalid_argument("mode must be 10 characters");

    uint32_t mode = 0;

    // File type
    switch (s[0]) {
    case '-': break; // regular
    case 'd': mode |= ModeDir; break;
    case 'l': mode |= ModeSymlink; break;
    case 'p': mode |= ModeNamedPipe; break;
    case 's': mode |= ModeSocket; break;
    case 'b': mode |= ModeDevice; break;
    case 'c': mode |= ModeCharDevice; break;
    default:
        throw std::invalid_argument(std::string("unknown file type: ") + s[0]);
    }

    // Permission bits
    if (s[1] == 'r') mode |= 0400;
    if (s[2] == 'w') mode |= 0200;
    if (s[3] == 'x' || s[3] == 's') mode |= 0100;
    if (s[4] == 'r') mode |= 0040;
    if (s[5] == 'w') mode |= 0020;
    if (s[6] == 'x' || s[6] == 's') mode |= 0010;
    if (s[7] == 'r') mode |= 0004;
    if (s[8] == 'w') mode |= 0002;
    if (s[9] == 'x' || s[9] == 't') mode |= 0001;

    // Special bits
    if (s[3] == 's' || s[3] == 'S') mode |= ModeSetuid;
    if (s[6] == 's' || s[6] == 'S') mode |= ModeSetgid;
    if (s[9] == 't' || s[9] == 'T') mode |= ModeSticky;

    return mode;
}

std::string FormatTimestamp(int64_t ts) {
    if (ts == NullTimestamp)
        return "-";

    time_t t = static_cast<time_t>(ts);
    struct tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec);
    return buf;
}

int64_t ParseTimestamp(const std::string &s) {
    if (s == "-" || s == "0")
        return NullTimestamp;

    int year, month, day, hour, minute, second;

    // Try canonical: 2006-01-02T15:04:05Z
    // Must verify 'Z' suffix — sscanf returns 6 even if trailing literal fails
    if (s.back() == 'Z' &&
        std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%dZ",
                    &year, &month, &day, &hour, &minute, &second) == 6) {
        struct tm t{};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
#ifdef _WIN32
        return static_cast<int64_t>(_mkgmtime(&t));
#else
        return static_cast<int64_t>(timegm(&t));
#endif
    }

    // Try RFC3339 with timezone offset: 2006-01-02T15:04:05+07:00
    int tz_h, tz_m;
    char tz_sign;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d",
                    &year, &month, &day, &hour, &minute, &second,
                    &tz_sign, &tz_h, &tz_m) == 9 &&
        (tz_sign == '+' || tz_sign == '-')) {
        struct tm t{};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
#ifdef _WIN32
        int64_t base = static_cast<int64_t>(_mkgmtime(&t));
#else
        int64_t base = static_cast<int64_t>(timegm(&t));
#endif
        int offset_sec = (tz_h * 3600 + tz_m * 60);
        if (tz_sign == '+') base -= offset_sec;
        else base += offset_sec;
        return base;
    }

    throw std::invalid_argument("cannot parse timestamp: " + s);
}

std::string Entry::Canonical() const {
    std::string line;

    // Mode
    bool is_null_mode = (mode == 0 && !IsDir() && !IsSymlink());
    if (is_null_mode)
        line += "----------";
    else
        line += FormatMode(mode);

    line += ' ';

    // Timestamp
    line += FormatTimestamp(timestamp);

    line += ' ';

    // Size
    if (size < 0)
        line += '-';
    else
        line += std::to_string(size);

    line += ' ';

    // Name
    line += formatName(name);

    // Symlink target
    if (!target.empty()) {
        line += " -> ";
        line += formatTarget(target);
    }

    // C4 ID
    if (!id.IsNil()) {
        line += ' ';
        line += id.String();
    }

    return line;
}

std::string Entry::Format(int indent_width) const {
    std::string indent(static_cast<size_t>(depth * indent_width), ' ');
    return indent + Canonical();
}

} // namespace c4m
