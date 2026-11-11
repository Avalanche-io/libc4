// SPDX-License-Identifier: Apache-2.0
// Internal: the one c4m entry-line parser, shared by Manifest::Parse and
// DecodePatchChain so both read every line by the same rules.

#pragma once

#include "c4/c4m.hpp"

#include <string>

namespace c4m::detail {

// Parse one entry from a full (indentation-included) line. indent_width is
// detected from the first indented line and updated in place. A line that
// does not parse throws std::runtime_error naming line_num.
Entry parseEntryFromLine(const std::string &line, int &indent_width, int line_num);

// True if name is a path, not a bare entry name. Only "/" separates paths
// in c4m, and a name may end in "/" only as a directory's marker: a name
// holding any other "/", and ".", "..", "./", "../", "/" and "", are path
// components (SPECIFICATION.md "Invalid names"; the shared name-rule
// vector). A backslash is a literal name character, never a separator.
// Go: c4m.isPathName. The one definition, used by the parser, the encoder
// and Manifest.
bool isPathName(const std::string &name);

} // namespace c4m::detail
