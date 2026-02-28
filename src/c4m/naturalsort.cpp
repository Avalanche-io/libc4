// SPDX-License-Identifier: Apache-2.0
// Natural sort comparison for c4m entry ordering.
// Numeric segments are compared as integers: "file2" < "file10"

#include "c4/c4m.hpp"

#include <cstdint>
#include <string>

namespace {

struct Segment {
    std::string text;
    bool is_numeric = false;
    int64_t num_value = 0;
};

std::vector<Segment> segmentString(const std::string &s) {
    if (s.empty())
        return {};

    std::vector<Segment> segments;
    std::string current;
    bool is_numeric = false;
    bool first = true;

    for (char c : s) {
        bool is_digit = (c >= '0' && c <= '9');

        if (first) {
            first = false;
            is_numeric = is_digit;
            current += c;
            continue;
        }

        if (is_digit != is_numeric) {
            // Transition: save current segment
            Segment seg;
            seg.text = current;
            seg.is_numeric = is_numeric;
            if (is_numeric) {
                seg.num_value = 0;
                for (char d : current)
                    seg.num_value = seg.num_value * 10 + (d - '0');
            }
            segments.push_back(std::move(seg));

            current.clear();
            current += c;
            is_numeric = is_digit;
        } else {
            current += c;
        }
    }

    // Final segment
    if (!current.empty()) {
        Segment seg;
        seg.text = current;
        seg.is_numeric = is_numeric;
        if (is_numeric) {
            seg.num_value = 0;
            for (char d : current)
                seg.num_value = seg.num_value * 10 + (d - '0');
        }
        segments.push_back(std::move(seg));
    }

    return segments;
}

} // anonymous namespace

namespace c4m {

bool NaturalLess(const std::string &a, const std::string &b) {
    auto segs_a = segmentString(a);
    auto segs_b = segmentString(b);

    size_t min_len = std::min(segs_a.size(), segs_b.size());

    for (size_t i = 0; i < min_len; i++) {
        const auto &sa = segs_a[i];
        const auto &sb = segs_b[i];

        if (sa.is_numeric && sb.is_numeric) {
            // Both numeric: compare as integers
            if (sa.num_value != sb.num_value)
                return sa.num_value < sb.num_value;
            // Equal values: shorter representation first
            if (sa.text.size() != sb.text.size())
                return sa.text.size() < sb.text.size();
        } else if (sa.is_numeric != sb.is_numeric) {
            // Mixed: text sorts before numeric
            return !sa.is_numeric;
        } else {
            // Both text: lexicographic
            if (sa.text != sb.text)
                return sa.text < sb.text;
        }
    }

    // All compared segments equal: shorter string first
    return segs_a.size() < segs_b.size();
}

} // namespace c4m
