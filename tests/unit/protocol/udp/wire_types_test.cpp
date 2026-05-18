// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/udp/wire_types.hpp"

namespace wire = pvpgn::protocol::udp::wire;

TEST_CASE("udp::wire constants match legacy values",
          "[protocol][udp][wire_types]")
{
    REQUIRE(wire::kServerUdpTest      == 0x00000005u);
    REQUIRE(wire::kClientUdpPing      == 0x00000007u);
    REQUIRE(wire::kClientSessionAddr1 == 0x00000008u);
    REQUIRE(wire::kClientSessionAddr2 == 0x00000009u);
    REQUIRE(wire::kBnetTag            == 0x626E6574u);
    REQUIRE(wire::kHeaderBytes        == 4);
}

TEST_CASE("udp::wire structs are trivially copyable and default-init",
          "[protocol][udp][wire_types]")
{
    static_assert(std::is_trivially_copyable_v<wire::UdpHeader>);
    static_assert(std::is_trivially_copyable_v<wire::ServerUdpTest>);
    static_assert(std::is_trivially_copyable_v<wire::ClientUdpPing>);
    static_assert(std::is_trivially_copyable_v<wire::ClientSessionAddr1>);
    static_assert(std::is_trivially_copyable_v<wire::ClientSessionAddr2>);

    wire::ServerUdpTest s{};
    REQUIRE(s.h.type    == 0);
    REQUIRE(s.bnettag   == wire::kBnetTag);

    wire::ClientSessionAddr2 a{};
    REQUIRE(a.h.type     == 0);
    REQUIRE(a.sessionkey == 0);
    REQUIRE(a.sessionnum == 0);
}
