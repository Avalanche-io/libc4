// SPDX-License-Identifier: Apache-2.0
// Internal: which inline ID lists an output carries, shared by
// Manifest::Parse (chain resolution) and ApplyPatch.

#pragma once

#include "c4/c4m.hpp"

#include <initializer_list>
#include <map>
#include <string>

namespace c4m::detail {

// keepRangeData sets on m the ID lists, taken from lists, of the folded
// ranges m holds: one per range whose list some input carries, and nothing
// else. A list no input carries stays absent; none is invented. Lists are
// keyed by their own C4 ID, so the same key in two inputs is the same list
// and their order does not matter. m must hold no lists yet. Go:
// c4m.keepRangeData (c4 #159; design/range-data-through-operations.md).
inline void keepRangeData(Manifest &m,
                          std::initializer_list<const std::map<c4::ID, std::string> *> lists) {
    for (const auto &e : m.Entries()) {
        if (!e.is_sequence || e.id.IsNil())
            continue;
        for (const auto *l : lists) {
            auto it = l->find(e.id);
            if (it == l->end())
                continue;
            m.SetRangeData(it->first, it->second);
            break;
        }
    }
}

} // namespace c4m::detail
