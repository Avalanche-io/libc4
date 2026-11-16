// SPDX-License-Identifier: Apache-2.0
// Internal: which inline ID lists an output carries, shared by every
// operation that builds a c4m from c4m inputs.

#pragma once

#include "c4/c4m.hpp"

#include <map>
#include <string>
#include <vector>

namespace c4m::detail {

using RangeLists = std::map<c4::ID, std::string>;

// rangeDataFor returns the ID lists, taken from lists, of the folded ranges
// among entries: one per range whose list some input carries, and nothing
// else. A list no input carries stays absent; none is invented. Lists are
// keyed by their own C4 ID, so the same key in two inputs is the same list
// and their order does not matter. skip, when it returns true, leaves an
// entry out (a patch's removal lines name ranges the result does not hold).
// Go: c4m.rangeDataFor (c4 #159; design/range-data-through-operations.md).
template <typename Skip>
RangeLists rangeDataFor(const std::vector<Entry> &entries, const std::vector<const RangeLists *> &lists,
                        Skip skip) {
    RangeLists out;
    for (const auto &e : entries) {
        if (!e.is_sequence || e.id.IsNil() || skip(e))
            continue;
        for (const auto *l : lists) {
            auto it = l->find(e.id);
            if (it == l->end())
                continue;
            out[it->first] = it->second;
            break;
        }
    }
    return out;
}

// keepRangeData sets on m the ID lists, taken from lists, of the folded
// ranges m holds, and no other. Every operation that writes a c4m from c4m
// inputs calls it on its output. m must hold no lists yet. Go:
// c4m.keepRangeData.
inline void keepRangeData(Manifest &m, const std::vector<const RangeLists *> &lists) {
    for (const auto &kv : rangeDataFor(m.Entries(), lists, [](const Entry &) { return false; }))
        m.SetRangeData(kv.first, kv.second);
}

} // namespace c4m::detail
