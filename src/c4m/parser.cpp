// SPDX-License-Identifier: Apache-2.0
// C4M format parser
// TODO: implement against c4m SPECIFICATION.md

#include "c4/c4m.hpp"

namespace c4m {

Manifest Manifest::Parse(std::string_view /*data*/) {
    // TODO: implement .c4m format parser
    // Reference: ~/ws/active/c4/c4/c4m/SPECIFICATION.md
    throw std::runtime_error("c4m parser not yet implemented");
}

Manifest Manifest::ParseFile(const std::filesystem::path &path) {
    // TODO: read file and delegate to Parse
    (void)path;
    throw std::runtime_error("c4m parser not yet implemented");
}

Manifest Manifest::Parse(std::istream &stream) {
    // TODO: streaming parser
    (void)stream;
    throw std::runtime_error("c4m parser not yet implemented");
}

} // namespace c4m
