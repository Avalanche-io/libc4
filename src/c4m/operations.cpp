// SPDX-License-Identifier: Apache-2.0
// C4M set operations: diff, merge, intersect, scan (stubs)

#include "c4/c4m.hpp"

#include <stdexcept>

namespace c4m {

Manifest Manifest::Diff(const Manifest &other) const {
    (void)other;
    throw std::runtime_error("c4m diff not yet implemented");
}

Manifest Manifest::Merge(const Manifest &other) const {
    (void)other;
    throw std::runtime_error("c4m merge not yet implemented");
}

Manifest Manifest::Intersect(const Manifest &other) const {
    (void)other;
    throw std::runtime_error("c4m intersection not yet implemented");
}

Manifest Manifest::Scan(const std::filesystem::path &dir) {
    (void)dir;
    throw std::runtime_error("c4m scan not yet implemented");
}

} // namespace c4m
