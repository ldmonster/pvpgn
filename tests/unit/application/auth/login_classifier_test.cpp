// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/auth/login_classifier.hpp"

using namespace pvpgn::application::auth;

namespace {

LoginAttemptInputs make_ok() {
    LoginAttemptInputs in{};
    in.account_exists       = true;
    in.password_matches     = true;
    in.locked               = false;
    in.must_change_password = false;
    in.banned_until         = 0;
    in.now                  = 1'000;
    return in;
}

}  // namespace

TEST_CASE("auth: missing account -> UnknownUser",
          "[application][auth][classifier]") {
    LoginAttemptInputs in{};
    in.now = 1'000;
    REQUIRE(classify_login_attempt(in) == LoginVerdict::UnknownUser);
}

TEST_CASE("auth: existing account, bad password -> BadPassword",
          "[application][auth][classifier]") {
    auto in = make_ok();
    in.password_matches = false;
    REQUIRE(classify_login_attempt(in) == LoginVerdict::BadPassword);
}

TEST_CASE("auth: locked account beats ban + must-change",
          "[application][auth][classifier]") {
    auto in = make_ok();
    in.locked               = true;
    in.banned_until         = 9'999;  // in the future
    in.must_change_password = true;
    REQUIRE(classify_login_attempt(in) == LoginVerdict::Locked);
}

TEST_CASE("auth: banned_until > now -> BannedTemporarily",
          "[application][auth][classifier]") {
    auto in = make_ok();
    in.banned_until = 2'000;
    REQUIRE(classify_login_attempt(in) == LoginVerdict::BannedTemporarily);
}

TEST_CASE("auth: banned_until == now -> ban window has elapsed (Ok)",
          "[application][auth][classifier]") {
    auto in = make_ok();
    in.banned_until = in.now;  // strictly greater is the rule
    REQUIRE(classify_login_attempt(in) == LoginVerdict::Ok);
}

TEST_CASE("auth: must_change_password is the lowest-priority rejection",
          "[application][auth][classifier]") {
    auto in = make_ok();
    in.must_change_password = true;
    REQUIRE(classify_login_attempt(in) == LoginVerdict::MustChangePassword);
}

TEST_CASE("auth: clean credentials -> Ok",
          "[application][auth][classifier]") {
    REQUIRE(classify_login_attempt(make_ok()) == LoginVerdict::Ok);
}

TEST_CASE("auth: to_login_error maps each verdict to LoginError",
          "[application][auth][classifier][mapping]") {
    REQUIRE_FALSE(to_login_error(LoginVerdict::Ok).has_value());
    REQUIRE(to_login_error(LoginVerdict::UnknownUser) == LoginError::UnknownUser);
    REQUIRE(to_login_error(LoginVerdict::BadPassword) == LoginError::InvalidCredentials);
    REQUIRE(to_login_error(LoginVerdict::Locked) == LoginError::Locked);
    REQUIRE(to_login_error(LoginVerdict::BannedTemporarily) == LoginError::Banned);
    REQUIRE(to_login_error(LoginVerdict::MustChangePassword)
            == LoginError::MustChangePassword);
}
