// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_test.cpp
/// Core unit tests for `application::connection::ConnectionFsm`, pairing
/// `connection_fsm.cpp` (dispatch / close / reply helpers). The per-state
/// handler tests live in the sibling `connection_fsm_<state>_test.cpp` files;
/// all six share `connection_fsm_test_fixtures.hpp`.
///
/// Covered here: initial state, dispatch routing of unknown / SID_NULL /
/// SID_PING packets, close() from every state, the Disconnecting drain, and
/// the full NLS / OLS login→chat integration flows that exercise dispatch
/// across all states.

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: initial state is Connecting", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx, 1u};

    CHECK(fsm.state() == ConnectionState::Connecting);
    CHECK(fsm.session_id() == 1u);
    CHECK(fsm.account_id() == 0u);
    CHECK(fsm.username().empty());
    CHECK(fsm.game_id() == 0u);
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.sent.empty());
}

// ---------------------------------------------------------------------------
// Unknown packet in any state → silently ignored (not disconnected)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: unknown packet in Connecting is silently ignored",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // SID 0xAB is not handled
    std::vector<std::byte> payload{std::byte{0xDE}, std::byte{0xAD}};
    auto result = fsm.dispatch(0xABu, std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Connecting);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: unknown packet in LoggedIn is silently ignored",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    std::vector<std::byte> payload{std::byte{0x00}};
    auto result = fsm.dispatch(0xFFu, std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// close() from each state → Disconnecting
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: close() from Connecting transitions to Disconnecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

TEST_CASE("ConnectionFsm: close() from LoggedIn transitions to Disconnecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

TEST_CASE("ConnectionFsm: close() from InChannel transitions to Disconnecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

TEST_CASE("ConnectionFsm: close() from InGame transitions to Disconnecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    reach_in_game_via_start(fsm);

    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// Packets in Disconnecting state are silently dropped
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: packets in Disconnecting state are silently dropped",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.close();
    ctx.sent.clear();

    auto payload = make_auth_info();
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    // Must succeed (not fail) — just silently dropped
    REQUIRE(result.has_value());
    CHECK(ctx.sent.empty());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
}

// ---------------------------------------------------------------------------
// SID_NULL (keepalive) is accepted in every state
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in Connecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});
    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Connecting);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in LoggedIn",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});
    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in InGame",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    reach_in_game_via_start(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});
    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// SID_PING echo — cookie is reflected verbatim
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_PING echoes cookie verbatim", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_ping(0xCAFEBABEu);
    auto result  = fsm.dispatch(sid::kPing,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kPing);
    // The reply body must contain the same 4-byte cookie
    REQUIRE(ctx.last()->payload.size() >= 4);
    const std::uint32_t echoed =
        static_cast<std::uint32_t>(ctx.last()->payload[0])
      | (static_cast<std::uint32_t>(ctx.last()->payload[1]) <<  8)
      | (static_cast<std::uint32_t>(ctx.last()->payload[2]) << 16)
      | (static_cast<std::uint32_t>(ctx.last()->payload[3]) << 24);
    CHECK(echoed == 0xCAFEBABEu);
    CHECK(fsm.state() == ConnectionState::Connecting);
}

// ---------------------------------------------------------------------------
// Full login→chat integration flows (dispatch across all states)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: full NLS flow reaches InChannel", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // AUTH_INFO
    auto ai = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // AUTH_CHECK (optional but common)
    auto ac = make_auth_check();
    REQUIRE(fsm.dispatch(sid::kAuthCheck,
                         std::span<const std::byte>{ac}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // ACCOUNTLOGON
    auto al = make_accountlogon("charlie");
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{al}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // ACCOUNTLOGONPROOF
    auto proof = make_accountlogonproof();
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogonProof,
                         std::span<const std::byte>{proof}).has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.username() == "charlie");

    // ENTERCHAT
    auto ec = make_enter_chat("charlie", "PXES");
    REQUIRE(fsm.dispatch(sid::kEnterChat,
                         std::span<const std::byte>{ec}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);

    CHECK_FALSE(ctx.closed);
    // Verify we got replies for AUTH_INFO, AUTH_CHECK, ACCOUNTLOGON, PROOF, ENTERCHAT
    CHECK(ctx.count_sid(sid::kAuthInfo)              >= 1);
    CHECK(ctx.count_sid(sid::kAuthCheck)             >= 1);
    CHECK(ctx.count_sid(sid::kAuthAccountLogon)      >= 1);
    CHECK(ctx.count_sid(sid::kAuthAccountLogonProof) >= 1);
    CHECK(ctx.count_sid(sid::kEnterChat)             >= 1);
}

TEST_CASE("ConnectionFsm: full OLS flow reaches InChannel", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // LOGON_REQUEST (legacy OLS single-step auth)
    reach_logged_in_ols(fsm, "bob");
    CHECK(fsm.username() == "bob");
    ctx.sent.clear();

    // ENTERCHAT
    reach_in_channel(fsm, "bob");

    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.count_sid(sid::kEnterChat) >= 1);
}
