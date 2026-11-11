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
// EscapeField applies SafeName encoding then c4m field-boundary escaping.
// Directory names: escape the base then re-append the trailing /.
// SafeName is applied once: applying it here as well escaped every
// SafeName escape a second time, so "trail\" was written "trail\\\\" and
// read back as "trail\\", a different name (backslash-name vector).
std::string formatName(const std::string &name, bool is_sequence) {
    if (!name.empty() && name.back() == '/')
        return c4m::EscapeField(name.substr(0, name.size() - 1), false) + "/";

    return c4m::EscapeField(name, is_sequence);
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
    // mode == 0 is null; the known zero is ZeroMode. Directory-ness comes
    // from the trailing "/" in the name, so a null-mode directory is still
    // a null.
    bool null_mode = IsNullMode(mode);
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

    // Each permission position holds its own letter or '-'; the owner- and
    // group-execute positions may also carry s/S (setuid, setgid) and the
    // other-execute position t/T (sticky). Any other character used to read
    // as a clear bit, so a damaged mode became a different mode with no
    // error (damaged-line vector).
    for (int i = 0; i < 9; i++) {
        char c = s[static_cast<size_t>(i) + 1];
        bool ok = c == '-';
        switch (i % 3) {
        case 0: ok = ok || c == 'r'; break;
        case 1: ok = ok || c == 'w'; break;
        default:
            ok = ok || c == 'x';
            if (i == 8) ok = ok || c == 't' || c == 'T';
            else ok = ok || c == 's' || c == 'S';
        }
        if (!ok)
            throw std::invalid_argument(std::string("invalid permission character '") + c +
                                        "' at position " + std::to_string(i + 2));
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

    // A ten-character mode is always known; all-zero is ZeroMode, not null.
    return KnownMode(mode);
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

// civilUTC converts a UTC civil time to Unix seconds, refusing a field out
// of range. timegm normalises out-of-range fields (month 13, day 45 became
// a date in the following year), so an impossible date read as a real one
// with no error (damaged-line vector).
static int64_t civilUTC(int year, int month, int day, int hour, int minute,
                        int second, const std::string &s) {
    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    int dim = (month >= 1 && month <= 12) ? mdays[month - 1] + (month == 2 && leap ? 1 : 0) : 0;
    if (month < 1 || month > 12 || day < 1 || day > dim || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59)
        throw std::invalid_argument("cannot parse timestamp: field out of range in " + s);
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

int64_t ParseTimestamp(const std::string &s) {
    if (s == "-" || s == "0")
        return NullTimestamp;

    int year, month, day, hour, minute, second;

    if (s.back() == 'Z' &&
        std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%dZ",
                    &year, &month, &day, &hour, &minute, &second) == 6) {
        return civilUTC(year, month, day, hour, minute, second, s);
    }

    int tz_h, tz_m;
    char tz_sign;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d",
                    &year, &month, &day, &hour, &minute, &second,
                    &tz_sign, &tz_h, &tz_m) == 9 &&
        (tz_sign == '+' || tz_sign == '-')) {
        int64_t base = civilUTC(year, month, day, hour, minute, second, s);
        int offset_sec = (tz_h * 3600 + tz_m * 60);
        if (tz_sign == '+') base -= offset_sec;
        else base += offset_sec;
        return base;
    }

    // Pretty format, as Go's pretty encoder writes it:
    // "Jan  2 15:04:05 2006 MST" or "Jan 2 15:04:05 2006 -0700".
    // The zone is UTC, GMT or Z (offset 0) or a numeric ±HHMM offset. Any
    // other abbreviation is ambiguous and is rejected rather than guessed:
    // a wrong offset would be an invented value that changes the ID.
    {
        char mon[4] = {0};
        char zone[8] = {0};
        if (std::sscanf(s.c_str(), "%3s %d %d:%d:%d %d %7s",
                        mon, &day, &hour, &minute, &second, &year, zone) == 7) {
            static const char *const months[12] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            month = 0;
            for (int i = 0; i < 12; i++) {
                if (std::strcmp(mon, months[i]) == 0) { month = i + 1; break; }
            }
            if (month == 0)
                throw std::invalid_argument("cannot parse timestamp: " + s);

            int64_t base = civilUTC(year, month, day, hour, minute, second, s);
            std::string z(zone);
            if (z == "UTC" || z == "GMT" || z == "Z")
                return base;
            if (z.size() == 5 && (z[0] == '+' || z[0] == '-') &&
                std::sscanf(z.c_str() + 1, "%2d%2d", &tz_h, &tz_m) == 2) {
                int offset_sec = (tz_h * 3600 + tz_m * 60);
                return (z[0] == '+') ? base - offset_sec : base + offset_sec;
            }
            throw std::invalid_argument("cannot parse timestamp: unknown zone in " + s);
        }
    }

    throw std::invalid_argument("cannot parse timestamp: " + s);
}

// Canonical form: null mode is "-" (single dash), no indentation.
// C4 ID or "-" is always the last field.
std::string Entry::Canonical() const {
    std::string line;

    // Mode: null renders as single "-"; the known zero (ZeroMode) as
    // "----------". Directory-ness comes from the trailing "/" in name.
    if (IsNullMode(mode))
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

// Format (display): includes indentation. Null mode is "-" here as in
// canonical form; the known zero is "----------". Writing null as ten
// dashes in display form conflated the two (C4M-STANDARD §4.1).
// C4 ID or "-" is always the last field.
std::string Entry::Format(int indent_width) const {
    std::string line(static_cast<size_t>(depth * indent_width), ' ');

    if (IsNullMode(mode))
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

} // namespace c4m
