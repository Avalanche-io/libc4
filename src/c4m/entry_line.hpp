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

} // namespace c4m::detail
