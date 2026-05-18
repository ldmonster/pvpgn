// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/init_wire_types.hpp"

namespace init = pvpgn::protocol::bnet::init;

TEST_CASE("init_wire_types constants match legacy CLIENT_INITCONN_CLASS_* values",
          "[protocol][bnet][init][wire_types]")
{
    REQUIRE(init::kClassBnet         == 0x01);
    REQUIRE(init::kClassFile         == 0x02);
    REQUIRE(init::kClassBot          == 0x03);
    REQUIRE(init::kClassEnc          == 0x04);
    REQUIRE(init::kClassTelnet       == 0x0d);
    REQUIRE(init::kClassD2cs         == 0x01);
    REQUIRE(init::kClassD2gs         == 0x64);
    REQUIRE(init::kClassD2csBnetd    == 0x65);
    REQUIRE(init::kClassLocalMachine == 0x98);
}

TEST_CASE("init_wire_types ClientInitConn is a single wire byte",
          "[protocol][bnet][init][wire_types]")
{
    static_assert(sizeof(init::ClientInitConn) == 1);
    static_assert(std::is_trivially_copyable_v<init::ClientInitConn>);

    init::ClientInitConn p{};
    REQUIRE(p.cclass == 0);

    p.cclass = init::kClassBnet;
    REQUIRE(p == init::ClientInitConn{0x01});
}
