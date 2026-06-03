// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_ingame_dispatch_test.cpp
/// Drives the SID dispatch paths of connection_fsm_ingame.cpp that the main
/// ingame test reaches only via the direct bind_d2_character() helper:
/// on_d2_char_select (SID_D2GAMELISTEX 0x68) and on_warcraft_general
/// (SID_WARCRAFTGENERAL 0x44, WAR3 route token), including their
/// short-payload and wrong-state early returns.

#include "connection_fsm_test_fixtures.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

using namespace pvpgn::test::connection_fsm;

namespace {

std::vector<std::byte> d2_char_select(std::uint8_t cls, std::uint8_t lvl,
                                      std::string_view name) {
    std::vector<std::byte> p;
    p.push_back(std::byte{cls});
    p.push_back(std::byte{lvl});
    push_cstr(p, name);
    return p;
}

std::vector<std::byte> warcraft_general(std::uint8_t subcmd, std::uint32_t token) {
    std::vector<std::byte> p;
    p.push_back(std::byte{subcmd});
    push_le32(p, token);
    return p;
}

}  // namespace

// --- on_d2_char_select -----------------------------------------------------

TEST_CASE("ConnectionFsm: SID_D2GAMELISTEX binds the D2 character",
          "[connection_fsm][ingame][d2]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    auto payload = d2_char_select(/*class*/ 5, /*level*/ 42, "Hardcore");
    auto result  = fsm.dispatch(sid::kD2CharSelect,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    REQUIRE(fsm.has_d2_character());
    CHECK(fsm.d2_char_name().value() == "Hardcore");
    CHECK(fsm.d2_char_class().value() == 5u);
    CHECK(fsm.d2_char_level().value() == 42u);
}

TEST_CASE("ConnectionFsm: SID_D2GAMELISTEX too short is ignored",
          "[connection_fsm][ingame][d2]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    std::vector<std::byte> payload{std::byte{1}, std::byte{2}};  // < 3 bytes
    auto result = fsm.dispatch(sid::kD2CharSelect,
                               std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK_FALSE(fsm.has_d2_character());
}

TEST_CASE("ConnectionFsm: SID_D2GAMELISTEX is ignored while Connecting",
          "[connection_fsm][ingame][d2]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};   // fresh -> Connecting

    auto payload = d2_char_select(5, 42, "Hardcore");
    auto result  = fsm.dispatch(sid::kD2CharSelect,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK_FALSE(fsm.has_d2_character());
}

// --- on_warcraft_general ---------------------------------------------------

TEST_CASE("ConnectionFsm: SID_WARCRAFTGENERAL stores the route token",
          "[connection_fsm][ingame][war3]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    auto payload = warcraft_general(/*subcmd*/ 0x00, /*token*/ 0xCAFEBABEu);
    auto result  = fsm.dispatch(sid::kWarcraftGeneral,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    REQUIRE(fsm.war3_route_token().has_value());
    CHECK(fsm.war3_route_token().value() == 0xCAFEBABEu);
}

TEST_CASE("ConnectionFsm: SID_WARCRAFTGENERAL too short sets no token",
          "[connection_fsm][ingame][war3]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    std::vector<std::byte> payload{std::byte{0}, std::byte{1}, std::byte{2}};  // < 5
    auto result = fsm.dispatch(sid::kWarcraftGeneral,
                               std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK_FALSE(fsm.war3_route_token().has_value());
}
