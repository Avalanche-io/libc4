// SPDX-License-Identifier: Apache-2.0
// Natural sort comparison for c4m entry ordering.
// Numeric segments are compared as integers: "file2" < "file10"
//
// This implementation walks both strings simultaneously without allocating
// any heap memory — no vectors, no substrings.

#include "c4/c4m.hpp"

#include <cstdint>

namespace c4m {

bool NaturalLess(const std::string &a, const std::string &b) {
    size_t ai = 0, bi = 0;
    size_t alen = a.size(), blen = b.size();

    while (ai < alen && bi < blen) {
        bool a_digit = (a[ai] >= '0' && a[ai] <= '9');
        bool b_digit = (b[bi] >= '0' && b[bi] <= '9');

        if (a_digit && b_digit) {
            // Both are numeric segments: compare as integers.
            // First, find the extent of each numeric run.
            size_t a_start = ai, b_start = bi;
            while (ai < alen && a[ai] >= '0' && a[ai] <= '9') ai++;
            while (bi < blen && b[bi] >= '0' && b[bi] <= '9') bi++;
            size_t a_len = ai - a_start;
            size_t b_len = bi - b_start;

            // Compare numeric values. For values that fit in int64, compare
            // directly. For longer runs, compare by length first (more digits
            // = larger number, since we don't have leading zeros in normal use,
            // but we handle them properly anyway).

            // Skip leading zeros for value comparison.
            size_t a_nz = a_start, b_nz = b_start;
            while (a_nz < ai && a[a_nz] == '0') a_nz++;
            while (b_nz < bi && b[b_nz] == '0') b_nz++;
            size_t a_siglen = ai - a_nz;
            size_t b_siglen = bi - b_nz;

            // Different number of significant digits = different value
            if (a_siglen != b_siglen) {
                return a_siglen < b_siglen;
            }

            // Same number of significant digits: compare digit by digit
            for (size_t k = 0; k < a_siglen; k++) {
                if (a[a_nz + k] != b[b_nz + k]) {
                    return a[a_nz + k] < b[b_nz + k];
                }
            }

            // Equal numeric value: shorter representation first (fewer leading zeros)
            if (a_len != b_len) {
                return a_len < b_len;
            }

            // Completely equal segment, continue to next
            continue;
        }

        if (a_digit != b_digit) {
            // Mixed: text sorts before numeric
            return !a_digit;
        }

        // Both are text characters: compare lexicographically
        if (a[ai] != b[bi]) {
            return a[ai] < b[bi];
        }
        ai++;
        bi++;
    }

    // All compared characters equal: shorter string first
    return alen < blen;
}

} // namespace c4m
