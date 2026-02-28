// SPDX-License-Identifier: Apache-2.0
// C4M format tests — placeholder until parser is implemented

#include "c4/c4m.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("C4M: entry types", "[c4m]") {
    c4m::Entry file_entry;
    file_entry.type = c4m::Entry::Type::File;
    REQUIRE_FALSE(file_entry.IsDir());
    REQUIRE_FALSE(file_entry.IsSequence());

    c4m::Entry dir_entry;
    dir_entry.type = c4m::Entry::Type::Dir;
    REQUIRE(dir_entry.IsDir());

    c4m::Entry seq_entry;
    seq_entry.type = c4m::Entry::Type::Sequence;
    REQUIRE(seq_entry.IsSequence());
}

TEST_CASE("C4M: empty manifest", "[c4m]") {
    c4m::Manifest m;
    REQUIRE(m.EntryCount() == 0);
    REQUIRE(m.Entries().empty());
    REQUIRE(m.AllEntries().empty());
}

// TODO: add parser tests once implemented
// Test vectors should be generated from the Go reference implementation
// to ensure cross-language compatibility.
