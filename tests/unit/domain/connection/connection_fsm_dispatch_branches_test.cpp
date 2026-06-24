// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_dispatch_branches_test.cpp
/// Core dispatch-routing branches of connection_fsm.cpp that the main
/// connection_fsm_test.cpp does not reach:
///   - SID_PING / SID_NULL routed from states other than Connecting/LoggedIn/InGame
///     (Authenticating, InChannel)
///   - the SID_LogonRequest2 (0x3A) alias routing to on_logon_request
///   - the SID_StartGame3 (0x1F) alias routing to on_start_game
///   - unknown-packet silent-ignore from Authenticating / InChannel / InGame
///   - send_result_reply / reply-encoding shape via the StartGame reply body
///   - close() from Authenticating, and the Disconnecting drain after a reject().

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;

// --- SID_NULL / SID_PING in the remaining (untested) states ------------------

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in Authenticating",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_authenticating(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);
    CHECK(ctx.sent.empty());   // SID_NULL emits nothing
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in InChannel",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(ctx.sent.empty());
}

TEST_CASE("ConnectionFsm: SID_PING echoes cookie while in InChannel",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_ping(0x0BADF00Du);
    auto result  = fsm.dispatch(sid::kPing,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kPing);
    REQUIRE(ctx.last()->payload.size() >= 4);
    const std::uint32_t echoed =
        static_cast<std::uint32_t>(ctx.last()->payload[0])
      | (static_cast<std::uint32_t>(ctx.last()->payload[1]) <<  8)
      | (static_cast<std::uint32_t>(ctx.last()->payload[2]) << 16)
      | (static_cast<std::uint32_t>(ctx.last()->payload[3]) << 24);
    CHECK(echoed == 0x0BADF00Du);
    CHECK(fsm.state() == ConnectionState::InChannel);
}

// --- SID_PING with a short payload (read_le32 past end) ----------------------
// read_le32 reads beyond the supplied bytes as zero; the echo must still send
// a 4-byte body and not crash.

TEST_CASE("ConnectionFsm: SID_PING with empty payload echoes a zero cookie",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto result = fsm.dispatch(sid::kPing, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kPing);
    REQUIRE(ctx.last()->payload.size() == 4);
}

// --- SID_LOGONRESPONSE2 (0x3A) alias routes to on_logon_request --------------

TEST_CASE("ConnectionFsm: SID_LOGONRESPONSE2 routes through on_logon_request",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_logon_request("carol");
    auto result  = fsm.dispatch(sid::kLogonRequest2,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);  // stub accepts
    CHECK(fsm.username() == "carol");
}

// --- SID_STARTADVEX3 (0x1F) alias routes to on_start_game --------------------

TEST_CASE("ConnectionFsm: SID_STARTADVEX3 routes through on_start_game",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_start_game("G3", "", "PXES");
    auto result  = fsm.dispatch(sid::kStartGame3,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    REQUIRE(ctx.last() != nullptr);
    // on_start_game always replies via sid::kStartGame1, even for the 0x1F alias.
    CHECK(ctx.last()->packet_id == sid::kStartGame1);
    REQUIRE(ctx.last()->payload.size() == 4);  // 4-byte result code
    // result code 0 == success
    CHECK(ctx.last()->payload[0] == std::byte{0});
}

// --- unknown packet silently ignored from each remaining state ---------------

TEST_CASE("ConnectionFsm: unknown packet in Authenticating is silently ignored",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_authenticating(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(0xEEu, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.sent.empty());
}

TEST_CASE("ConnectionFsm: unknown packet in InChannel is silently ignored",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(0x7Bu, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: unknown packet in InGame is silently ignored",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    reach_in_game_via_start(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(0xC3u, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK_FALSE(ctx.closed);
}

// --- close() from Authenticating --------------------------------------------

TEST_CASE("ConnectionFsm: close() from Authenticating transitions to Disconnecting",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_authenticating(fsm);

    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// --- after a reject(), subsequent packets are dropped (Disconnecting drain) --

TEST_CASE("ConnectionFsm: packets after a reject() are silently dropped",
          "[connection_fsm][dispatch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // SID_CHATCOMMAND from Connecting rejects -> Disconnecting.
    auto chat = make_chat_command("hi");
    auto bad  = fsm.dispatch(sid::kChatCommand,
                             std::span<const std::byte>{chat});
    REQUIRE_FALSE(bad.has_value());
    REQUIRE(fsm.state() == ConnectionState::Disconnecting);
    ctx.sent.clear();

    // A subsequent well-formed packet is dropped, returning ok().
    auto ping = make_ping();
    auto result = fsm.dispatch(sid::kPing, std::span<const std::byte>{ping});

    REQUIRE(result.has_value());
    CHECK(ctx.sent.empty());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
}
