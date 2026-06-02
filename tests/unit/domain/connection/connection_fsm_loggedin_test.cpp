// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_loggedin_test.cpp
/// Unit tests for the LoggedIn-state handler, pairing
/// `connection_fsm_loggedin.cpp` (on_enter_chat). Covers the
/// LoggedIn → InChannel transition on SID_ENTERCHAT and its rejection when the
/// client has not yet logged in.

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;

TEST_CASE("ConnectionFsm: SID_ENTERCHAT transitions LoggedIn→InChannel",
          "[connection_fsm][loggedin]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm, "alice");
    ctx.sent.clear();

    auto payload = make_enter_chat("alice", "PXES");
    auto result  = fsm.dispatch(sid::kEnterChat,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    // FSM must send SID_ENTERCHAT reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kEnterChat);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_ENTERCHAT in Connecting state is rejected",
          "[connection_fsm][loggedin]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_enter_chat("alice");
    auto result  = fsm.dispatch(sid::kEnterChat,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}
