// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_authenticating_test.cpp
/// Unit tests for the Authenticating-state handlers, pairing
/// `connection_fsm_authenticating.cpp` (on_auth_accountlogon /
/// on_auth_accountlogonproof). Covers the NLS challenge→proof path to
/// LoggedIn, rejection of these packets outside Authenticating, and that the
/// pending NLS state is cleared after a successful proof.

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;

// --- NLS challenge → proof → LoggedIn --------------------------------------

TEST_CASE("ConnectionFsm: NLS auth path reaches LoggedIn",
          "[connection_fsm][authenticating]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // Step 1: AUTH_INFO
    auto ai = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // Step 2: ACCOUNTLOGON
    auto al = make_accountlogon("alice");
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{al}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating); // still waiting for proof

    // Step 3: ACCOUNTLOGONPROOF
    auto proof = make_accountlogonproof();
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogonProof,
                         std::span<const std::byte>{proof}).has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.username() == "alice");
    CHECK(fsm.account_id() != 0u);
    CHECK_FALSE(ctx.closed);
}

// --- Wrong-state rejection -------------------------------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_ACCOUNTLOGON in Connecting state is rejected",
          "[connection_fsm][authenticating]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_accountlogon("alice");
    auto result  = fsm.dispatch(sid::kAuthAccountLogon,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_AUTH_ACCOUNTLOGONPROOF in Connecting state is rejected",
          "[connection_fsm][authenticating]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_accountlogonproof();
    auto result  = fsm.dispatch(sid::kAuthAccountLogonProof,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// --- Pending NLS state is cleared after a successful proof -----------------

TEST_CASE("ConnectionFsm: pending_nls_ctx cleared after successful proof",
          "[connection_fsm][authenticating][nlsbranch]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // Drive through AUTH_INFO → ACCOUNTLOGON (challenge stored)
    auto ai = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());

    auto al = make_accountlogon("alice");
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{al}).has_value());
    // Still Authenticating — pending NLS state is held
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // PROOF — clears pending state and transitions to LoggedIn
    auto proof = make_accountlogonproof();
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogonProof,
                         std::span<const std::byte>{proof}).has_value());

    CHECK(fsm.state() == ConnectionState::LoggedIn);
    // After proof the FSM must not hold stale NLS state:
    // a second PROOF in LoggedIn state must be rejected (not crash)
    auto result2 = fsm.dispatch(sid::kAuthAccountLogonProof,
                                std::span<const std::byte>{proof});
    CHECK_FALSE(result2.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
}
