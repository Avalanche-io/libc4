// SPDX-License-Identifier: Apache-2.0
// SafeName: Universal Filename Encoding (three-tier system).
// Matches Go reference: github.com/Avalanche-io/c4/c4m/safename.go

#include "c4/c4m.hpp"

#include <string>

namespace {

// Returns true if the rune is a printable character (visible or space).
// Matches Go's unicode.IsPrint.
bool isPrintableRune(uint32_t cp) {
    if (cp < 0x20) return false;              // Control chars
    if (cp == 0x7F) return false;             // DEL
    if (cp >= 0x80 && cp <= 0x9F) return false; // C1 controls
    if (cp == 0xFFFD) return false;           // Replacement char (used for errors)
    return true;
}

// Decode one UTF-8 rune from s starting at pos.
// Returns the codepoint and advances pos. On error, returns 0xFFFD and advances by 1.
uint32_t decodeUTF8(const std::string &s, size_t &pos) {
    uint8_t b0 = static_cast<uint8_t>(s[pos]);
    if (b0 < 0x80) {
        pos++;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && pos + 1 < s.size()) {
        uint8_t b1 = static_cast<uint8_t>(s[pos + 1]);
        if ((b1 & 0xC0) == 0x80) {
            uint32_t cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
            if (cp >= 0x80) {
                pos += 2;
                return cp;
            }
        }
    }
    if ((b0 & 0xF0) == 0xE0 && pos + 2 < s.size()) {
        uint8_t b1 = static_cast<uint8_t>(s[pos + 1]);
        uint8_t b2 = static_cast<uint8_t>(s[pos + 2]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
            uint32_t cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            if (cp >= 0x800 && (cp < 0xD800 || cp > 0xDFFF)) {
                pos += 3;
                return cp;
            }
        }
    }
    if ((b0 & 0xF8) == 0xF0 && pos + 3 < s.size()) {
        uint8_t b1 = static_cast<uint8_t>(s[pos + 1]);
        uint8_t b2 = static_cast<uint8_t>(s[pos + 2]);
        uint8_t b3 = static_cast<uint8_t>(s[pos + 3]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
            uint32_t cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) |
                          ((b2 & 0x3F) << 6) | (b3 & 0x3F);
            if (cp >= 0x10000 && cp <= 0x10FFFF) {
                pos += 4;
                return cp;
            }
        }
    }
    // Invalid UTF-8 byte
    pos++;
    return 0xFFFD;
}

// Encode a codepoint as UTF-8 and append to out.
void encodeUTF8(std::string &out, uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Currency sign U+00A4
constexpr uint32_t CurrencySign = 0x00A4;

// Tier 2 escape: returns the escape char for a codepoint, or 0 if not tier 2.
char tier2Escape(uint32_t cp) {
    switch (cp) {
    case 0x00: return '0';
    case '\t': return 't';
    case '\n': return 'n';
    case '\r': return 'r';
    case '\\': return '\\';
    }
    return 0;
}

// Tier 2 unescape: returns the byte value for an escape char.
// Returns -1 if not a valid tier 2 escape.
int tier2Unescape(char c) {
    switch (c) {
    case '0': return 0x00;
    case 't': return '\t';
    case 'n': return '\n';
    case 'r': return '\r';
    case '\\': return '\\';
    }
    return -1;
}

} // anonymous namespace

namespace c4m {

std::string SafeName(const std::string &raw) {
    // Fast path: check if encoding is needed.
    bool safe = true;
    {
        size_t pos = 0;
        while (pos < raw.size()) {
            uint32_t cp = decodeUTF8(raw, pos);
            if (cp == 0xFFFD || cp == CurrencySign || cp == '\\' || !isPrintableRune(cp)) {
                safe = false;
                break;
            }
        }
    }
    if (safe) return raw;

    std::string out;
    out.reserve(raw.size());
    std::string pending; // Tier 3 byte accumulator

    auto flushPending = [&]() {
        if (pending.empty()) return;
        encodeUTF8(out, CurrencySign);
        for (uint8_t b : pending) {
            encodeUTF8(out, 0x2800 + b);
        }
        encodeUTF8(out, CurrencySign);
        pending.clear();
    };

    size_t pos = 0;
    while (pos < raw.size()) {
        size_t start = pos;
        uint32_t cp = decodeUTF8(raw, pos);

        // Tier 1: printable UTF-8, not currency sign, not backslash.
        if (cp != 0xFFFD && isPrintableRune(cp) && cp != CurrencySign && cp != '\\') {
            flushPending();
            out.append(raw, start, pos - start);
            continue;
        }

        // Tier 2: backslash escapes for specific characters.
        char esc = (cp != 0xFFFD) ? tier2Escape(cp) : 0;
        if (esc != 0) {
            flushPending();
            out += '\\';
            out += esc;
            continue;
        }

        // Tier 3: accumulate raw bytes.
        if (cp == 0xFFFD) {
            // Single invalid byte
            pending += raw[start];
        } else {
            for (size_t i = start; i < pos; i++) {
                pending += raw[i];
            }
        }
    }
    flushPending();

    return out;
}

std::string UnsafeName(const std::string &encoded) {
    if (encoded.find('\\') == std::string::npos && encoded.find('\xC2') == std::string::npos) {
        // Quick check: currency sign in UTF-8 is C2 A4; if no backslash and no 0xC2
        // byte, no encoding present.
        return encoded;
    }

    std::string out;
    out.reserve(encoded.size());

    size_t pos = 0;
    while (pos < encoded.size()) {
        size_t start = pos;
        uint32_t cp = decodeUTF8(encoded, pos);

        // Tier 2: backslash escape.
        if (cp == '\\') {
            if (pos < encoded.size()) {
                char next = encoded[pos];
                int val = tier2Unescape(next);
                if (val >= 0) {
                    out += static_cast<char>(val);
                    pos++;
                    continue;
                }
            }
            // Lone backslash or unknown escape: pass through.
            out += '\\';
            continue;
        }

        // Tier 3: currency sign delimited braille range.
        if (cp == CurrencySign) {
            size_t j = pos;
            bool decoded = false;
            std::string bytes;
            while (j < encoded.size()) {
                size_t jstart = j;
                uint32_t br = decodeUTF8(encoded, j);
                if (br == CurrencySign) {
                    if (decoded) {
                        out += bytes;
                        pos = j;
                    } else {
                        encodeUTF8(out, CurrencySign);
                    }
                    goto next;
                }
                if (br >= 0x2800 && br <= 0x28FF) {
                    bytes += static_cast<char>(br - 0x2800);
                    decoded = true;
                    continue;
                }
                // Not braille, stop.
                break;
            }
            encodeUTF8(out, CurrencySign);
            continue;
        }

        // Tier 1: passthrough.
        out.append(encoded, start, pos - start);
        continue;

    next:;
    }

    return out;
}

std::string EscapeField(const std::string &name, bool is_sequence) {
    std::string safe = SafeName(name);

    bool needs = false;
    for (char c : safe) {
        if (c == ' ' || c == '"') { needs = true; break; }
        if (!is_sequence && (c == '[' || c == ']')) { needs = true; break; }
    }
    if (!needs) return safe;

    std::string out;
    out.reserve(safe.size() + 4);
    for (char c : safe) {
        switch (c) {
        case ' ':  out += "\\ "; break;
        case '"':  out += "\\\""; break;
        case '[':
            if (!is_sequence) out += "\\[";
            else out += c;
            break;
        case ']':
            if (!is_sequence) out += "\\]";
            else out += c;
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

std::string UnescapeField(const std::string &escaped) {
    if (escaped.find('\\') == std::string::npos) return escaped;

    std::string out;
    out.reserve(escaped.size());
    for (size_t i = 0; i < escaped.size(); i++) {
        if (escaped[i] == '\\' && i + 1 < escaped.size()) {
            char next = escaped[i + 1];
            if (next == ' ' || next == '"' || next == '[' || next == ']') {
                out += next;
                i++;
                continue;
            }
        }
        out += escaped[i];
    }
    return out;
}

} // namespace c4m
