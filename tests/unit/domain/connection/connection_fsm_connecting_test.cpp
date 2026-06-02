// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_connecting_test.cpp
/// Unit tests for the Connecting-state handlers, pairing
/// `connection_fsm_connecting.cpp` (on_auth_info / on_auth_check /
/// on_logon_request). Covers the AUTH_INFO and legacy OLS LOGON_REQUEST entry
/// transitions, wrong-state rejection, the AUTH_INFO reply's logon_type field,
/// and the product-tag → NLS/OLS branching set from AUTH_INFO.

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;

// --- SID_AUTH_INFO: Connecting → Authenticating ----------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_INFO transitions Connecting→Authenticating",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_auth_info(0x52415453u /* STAR */);
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);
    // FSM must send a SID_AUTH_INFO reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kAuthInfo);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_AUTH_INFO in Authenticating state is rejected",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_authenticating(fsm);
    ctx.sent.clear();

    // Second AUTH_INFO while already Authenticating → reject
    auto payload = make_auth_info();
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_AUTH_INFO in LoggedIn state is rejected",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_auth_info();
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_AUTH_INFO reply has logon_type in body",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{payload}).has_value());

    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kAuthInfo);
    // First 4 bytes of reply body = logon_type (LE uint32)
    REQUIRE(ctx.last()->payload.size() >= 4);
    const std::uint32_t logon_type =
        static_cast<std::uint32_t>(ctx.last()->payload[0])
      | (static_cast<std::uint32_t>(ctx.last()->payload[1]) <<  8)
      | (static_cast<std::uint32_t>(ctx.last()->payload[2]) << 16)
      | (static_cast<std::uint32_t>(ctx.last()->payload[3]) << 24);
    // logon_type 2 = NLS (SRP)
    CHECK(logon_type == 2u);
}

// --- SID_LOGON_REQUEST: legacy OLS Connecting → LoggedIn -------------------

TEST_CASE("ConnectionFsm: SID_LOGON_REQUEST transitions Connecting→LoggedIn",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_logon_request("bob");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.username() == "bob");
    // FSM must send a reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kLogonRequest);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_LOGON_REQUEST in Authenticating state is rejected",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_authenticating(fsm);
    ctx.sent.clear();

    auto payload = make_logon_request("bob");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// --- Product-tag → NLS/OLS branching (set from AUTH_INFO) ------------------

TEST_CASE("ConnectionFsm: is_nls_client false before AUTH_INFO",
          "[connection_fsm][connecting][nlsbranch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    CHECK_FALSE(fsm.is_nls_client());
}

TEST_CASE("ConnectionFsm: is_nls_client true after AUTH_INFO with WAR3 tag",
          "[connection_fsm][connecting][nlsbranch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // WAR3 product tag = 0x57415233
    auto payload = make_auth_info(kTagWar3);
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{payload}).has_value());

    CHECK(fsm.is_nls_client());
    CHECK(fsm.client_product_tag() == kTagWar3);
}

TEST_CASE("ConnectionFsm: is_nls_client true after AUTH_INFO with W3XP tag",
          "[connection_fsm][connecting][nlsbranch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // W3XP product tag = 0x57335850
    auto payload = make_auth_info(kTagW3xp);
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{payload}).has_value());

    CHECK(fsm.is_nls_client());
    CHECK(fsm.client_product_tag() == kTagW3xp);
}

TEST_CASE("ConnectionFsm: is_nls_client false after AUTH_INFO with STAR tag",
          "[connection_fsm][connecting][nlsbranch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // STAR product tag = 0x52415453 (OLS client)
    auto payload = make_auth_info(0x52415453u);
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{payload}).has_value());

    CHECK_FALSE(fsm.is_nls_client());
}
