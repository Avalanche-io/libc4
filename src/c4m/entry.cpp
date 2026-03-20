// SPDX-License-Identifier: Apache-2.0
// C4M entry: mode/timestamp formatting, canonical form.
// Matches Go reference: github.com/Avalanche-io/c4/c4m/entry.go

#include "c4/c4m.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>

namespace {

// Format name for c4m output using backslash escaping (no quoting).
// Applies SafeName encoding then c4m field-boundary escaping.
// Directory names: escape the base then re-append the trailing /.
std::string formatName(const std::string &name, bool is_sequence) {
    std::string safe = c4m::SafeName(name);

    if (!name.empty() && name.back() == '/') {
        std::string base = safe.substr(0, safe.size() - 1);
        return c4m::EscapeField(base, false) + "/";
    }

    return c4m::EscapeField(safe, is_sequence);
}

// Format symlink target for c4m output.
// Targets don't get bracket escaping since brackets in paths are literal.
std::string formatTarget(const std::string &t) {
    std::string safe = c4m::SafeName(t);
    bool needs = false;
    for (char c : safe) {
        if (c == ' ' || c == '"') { needs = true; break; }
    }
    if (!needs) return safe;

    std::string out;
    out.reserve(safe.size() + 4);
    for (char c : safe) {
        if (c == ' ') out += "\\ ";
        else if (c == '"') out += "\\\"";
        else out += c;
    }
    return out;
}

} // anonymous namespace

namespace c4m {

bool Entry::IsDir() const {
    return (mode & ModeDir) != 0 ||
           (!name.empty() && name.back() == '/');
}

bool Entry::IsSymlink() const {
    return (mode & ModeSymlink) != 0;
}

bool Entry::HasNullValues() const {
    bool null_mode = (mode == 0 && !IsDir() && !IsSymlink());
    bool null_ts = (timestamp == NullTimestamp);
    bool null_sz = (size < 0);
    return null_mode || null_ts || null_sz;
}

std::string Entry::FlowOperator() const {
    switch (flow_direction) {
    case FlowDirection::Outbound: return "->";
    case FlowDirection::Inbound: return "<-";
    case FlowDirection::Bidirectional: return "<>";
    default: return "";
    }
}

std::string FormatMode(uint32_t mode) {
    char buf[11];

    uint32_t type_bits = mode & ModeTypeMask;
    if (type_bits == 0)                buf[0] = '-';
    else if (type_bits & ModeDir)        buf[0] = 'd';
    else if (type_bits & ModeSymlink)    buf[0] = 'l';
    else if (type_bits & ModeNamedPipe)  buf[0] = 'p';
    else if (type_bits & ModeSocket)     buf[0] = 's';
    else if (type_bits & ModeDevice)     buf[0] = 'b';
    else if (type_bits & ModeCharDevice) buf[0] = 'c';
    else                                 buf[0] = '?';

    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) {
        if (mode & (1u << (8 - i)))
            buf[i + 1] = rwx[i];
        else
            buf[i + 1] = '-';
    }

    if (mode & ModeSetuid) buf[3] = (buf[3] == 'x') ? 's' : 'S';
    if (mode & ModeSetgid) buf[6] = (buf[6] == 'x') ? 's' : 'S';
    if (mode & ModeSticky) buf[9] = (buf[9] == 'x') ? 't' : 'T';

    buf[10] = '\0';
    return buf;
}

uint32_t ParseMode(const std::string &s) {
    if (s.size() != 10)
        throw std::invalid_argument("mode must be 10 characters");

    uint32_t mode = 0;

    switch (s[0]) {
    case '-': break;
    case 'd': mode |= ModeDir; break;
    case 'l': mode |= ModeSymlink; break;
    case 'p': mode |= ModeNamedPipe; break;
    case 's': mode |= ModeSocket; break;
    case 'b': mode |= ModeDevice; break;
    case 'c': mode |= ModeCharDevice; break;
    default:
        throw std::invalid_argument(std::string("unknown file type: ") + s[0]);
    }

    if (s[1] == 'r') mode |= 0400;
    if (s[2] == 'w') mode |= 0200;
    if (s[3] == 'x' || s[3] == 's') mode |= 0100;
    if (s[4] == 'r') mode |= 0040;
    if (s[5] == 'w') mode |= 0020;
    if (s[6] == 'x' || s[6] == 's') mode |= 0010;
    if (s[7] == 'r') mode |= 0004;
    if (s[8] == 'w') mode |= 0002;
    if (s[9] == 'x' || s[9] == 't') mode |= 0001;

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

// Canonical form: null mode is "-" (single dash), no indentation.
// C4 ID or "-" is always the last field.
std::string Entry::Canonical() const {
    std::string line;

    // Mode: null renders as single "-"
    bool is_null_mode = (mode == 0 && !IsDir() && !IsSymlink());
    if (is_null_mode)
        line += '-';
    else
        line += FormatMode(mode);

    line += ' ';
    line += FormatTimestamp(timestamp);
    line += ' ';

    if (size < 0)
        line += '-';
    else
        line += std::to_string(size);

    line += ' ';
    line += formatName(name, is_sequence);

    // Symlink target, hard link marker, or flow link
    if (!target.empty()) {
        line += " -> ";
        line += formatTarget(target);
    } else if (hard_link != 0) {
        if (hard_link < 0) {
            line += " ->";
        } else {
            line += " ->";
            line += std::to_string(hard_link);
        }
    } else if (flow_direction != FlowDirection::None) {
        line += ' ';
        line += FlowOperator();
        line += ' ';
        line += flow_target;
    }

    // C4 ID or "-" is always the last field
    if (!id.IsNil()) {
        line += ' ';
        line += id.String();
    } else {
        line += " -";
    }

    return line;
}

// Format (display): null mode is "----------", includes indentation.
// C4 ID or "-" is always the last field.
std::string Entry::Format(int indent_width) const {
    std::string line(static_cast<size_t>(depth * indent_width), ' ');

    // Mode: null renders as "----------" in display format
    bool is_null_mode = (mode == 0 && !IsDir() && !IsSymlink());
    if (is_null_mode)
        line += "----------";
    else
        line += FormatMode(mode);

    line += ' ';
    line += FormatTimestamp(timestamp);
    line += ' ';

    if (size < 0)
        line += '-';
    else
        line += std::to_string(size);

    line += ' ';
    line += formatName(name, is_sequence);

    // Symlink target, hard link marker, or flow link
    if (!target.empty()) {
        line += " -> ";
        line += formatTarget(target);
    } else if (hard_link != 0) {
        if (hard_link < 0) {
            line += " ->";
        } else {
            line += " ->";
            line += std::to_string(hard_link);
        }
    } else if (flow_direction != FlowDirection::None) {
        line += ' ';
        line += FlowOperator();
        line += ' ';
        line += flow_target;
    }

    // C4 ID or "-" is always the last field
    if (!id.IsNil()) {
        line += ' ';
        line += id.String();
    } else {
        line += " -";
    }

    return line;
}

} // namespace c4m
