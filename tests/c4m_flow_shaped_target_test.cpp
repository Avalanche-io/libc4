// SPDX-License-Identifier: Apache-2.0
// A link whose target has the shape of a flow target (`host:path`, `C:foo`)
// cannot be told from a flow link when the mode is null. Ruled 2026-11-13
// (decided by the release lead; reversible; link-target-vector README, c4
// #158 at 7cdc9f3): under a null mode the line is a flow link, and under
// lrwxrwxrwx it is the symlink. A writer keeps a symlink's type by writing
// that mode even when the rest of the mode is null. This mirrors Go's
// TestAFlowShapedTargetIsALinkOnlyUnderASymlinkMode, building each line
// through the API.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

c4m::Manifest roundTrip(uint32_t mode, const std::string &target, const c4::ID &id) {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = "link";
    e.mode = mode;
    e.size = c4m::NullSize;
    e.timestamp = c4m::NullTimestamp;
    e.target = target;
    e.id = id;
    m.AddEntry(e);
    return c4m::Manifest::Parse(std::string_view(m.Encode()));
}

} // namespace

TEST_CASE("link: a flow-shaped target is a symlink only under a symlink mode", "[c4m][link][flow]") {
    const c4::ID content = c4::ID::Identify(std::string_view("what the link points at"));
    for (const char *target : {"host:path", "C:foo", "studio:inbox/"}) {
        for (bool withID : {false, true}) {
            INFO(target << (withID ? " with a C4 ID" : " without a C4 ID"));
            const c4::ID id = withID ? content : c4::ID();

            // Under lrwxrwxrwx the line reads back as the symlink.
            auto back = roundTrip(c4m::ModeSymlink | 0777, target, id);
            REQUIRE(back.EntryCount() == 1);
            const auto &s = back.Entries()[0];
            CHECK(s.target == target);
            CHECK(s.flow_direction == c4m::FlowDirection::None);
            CHECK(s.IsSymlink());

            // Under a null mode the same line is a flow link to that location.
            back = roundTrip(0, target, id);
            REQUIRE(back.EntryCount() == 1);
            const auto &f = back.Entries()[0];
            CHECK(f.flow_direction == c4m::FlowDirection::Outbound);
            CHECK(f.flow_target == target);
            CHECK(f.target.empty());
        }
    }
}
