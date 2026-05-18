// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/wire_types.hpp"

namespace b = pvpgn::protocol::bnet;

TEST_CASE("bnet BnetHeader has 4-byte wire layout",
          "[protocol][bnet][wire_types]")
{
    REQUIRE(sizeof(b::BnetHeader) == 4);
    REQUIRE(std::is_trivially_copyable_v<b::BnetHeader>);

    b::BnetHeader h{0x0aff, 0x0010};
    REQUIRE(h.type == 0x0aff);
    REQUIRE(h.size == 0x0010);
    REQUIRE(h == b::BnetHeader{0x0aff, 0x0010});
    REQUIRE_FALSE(h == b::BnetHeader{0x0aff, 0x0011});
}

TEST_CASE("bnet W3RouteHeader has 4-byte wire layout",
          "[protocol][bnet][wire_types]")
{
    REQUIRE(sizeof(b::W3RouteHeader) == 4);
    REQUIRE(std::is_trivially_copyable_v<b::W3RouteHeader>);

    b::W3RouteHeader h{0x1ef7, 0x0035};
    REQUIRE(h.type == 0x1ef7);
    REQUIRE(h.size == 0x0035);
}

TEST_CASE("bnet generic wrappers and CLIENT_NULL pin legacy values",
          "[protocol][bnet][wire_types]")
{
    REQUIRE(sizeof(b::BnetGeneric) == 4);
    REQUIRE(sizeof(b::W3RouteGeneric) == 4);
    REQUIRE(b::kClientNull == 0xfeff);
}
