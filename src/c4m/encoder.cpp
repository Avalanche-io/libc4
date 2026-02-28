// SPDX-License-Identifier: Apache-2.0
// C4M format encoder
// TODO: implement against c4m SPECIFICATION.md

#include "c4/c4m.hpp"

namespace c4m {

std::string Manifest::Encode() const {
    // TODO: implement .c4m format encoder
    throw std::runtime_error("c4m encoder not yet implemented");
}

void Manifest::WriteFile(const std::filesystem::path &path) const {
    (void)path;
    throw std::runtime_error("c4m encoder not yet implemented");
}

} // namespace c4m
