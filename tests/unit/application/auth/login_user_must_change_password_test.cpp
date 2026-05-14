// SPDX-License-Identifier: GPL-2.0-or-later
//
// Batch 22e: integration-style scenario that walks the must-change-
// password arm of the login pipeline end-to-end without leaning on
// the domain aggregate.
//
// The use-case `LoginUser` cannot itself emit
// `LoginError::MustChangePassword` today because `identity::Account`
// does not (yet) model password-rotation policy. The legacy bridge
// path however does: it inspects the legacy "PASS_CHANGE" flag,
// fabricates a `LoginVerdict::MustChangePassword`, and uses
// `to_login_error` to plumb it into the use-case's error vocabulary.
//
// This test pins that plumbing so the legacy → application-layer
// translation cannot regress when the domain aggregate later gains
// the rotation field and the bridge is removed.

#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/login_classifier.hpp"
#include "application/auth/login_user.hpp"

using pvpgn::application::auth::LoginAttemptInputs;
using pvpgn::application::auth::LoginError;
using pvpgn::application::auth::LoginVerdict;
using pvpgn::application::auth::classify_login_attempt;
using pvpgn::application::auth::to_login_error;

namespace {

LoginAttemptInputs make_ok_input() {
    LoginAttemptInputs in{};
    in.account_exists = true;
    in.password_matches = true;
    in.locked = false;
    in.banned_until = 0;
    in.must_change_password = false;
    in.now = 100;
    return in;
}

}  // namespace

TEST_CASE(
    "login_user (bridge): MustChangePassword verdict surfaces as "
    "LoginError::MustChangePassword via the classifier seam",
    "[application][auth][login_user][must_change_password]") {
    // Step 1: legacy adapter fabricates the classifier input from a
    // stub account whose only abnormal trait is the rotation flag.
    auto in = make_ok_input();
    in.must_change_password = true;

    // Step 2: domain-pure classification yields the verdict.
    auto verdict = classify_login_attempt(in);
    REQUIRE(verdict == LoginVerdict::MustChangePassword);

    // Step 3: error vocabulary the bridge would return to the
    // protocol layer matches the use-case-facing enum exactly.
    auto err = to_login_error(verdict);
    REQUIRE(err.has_value());
    REQUIRE(err.value() == LoginError::MustChangePassword);
}

TEST_CASE(
    "login_user (bridge): must_change_password is the lowest-priority "
    "rejection so happy-path inputs are unaffected",
    "[application][auth][login_user][must_change_password]") {
    auto in = make_ok_input();
    // No must_change_password flag set => no error.
    auto verdict = classify_login_attempt(in);
    REQUIRE(verdict == LoginVerdict::Ok);
    REQUIRE_FALSE(to_login_error(verdict).has_value());
}

TEST_CASE(
    "login_user (bridge): higher-priority rejections shadow "
    "must_change_password",
    "[application][auth][login_user][must_change_password]") {
    auto in = make_ok_input();
    in.must_change_password = true;
    // Locked outranks must_change_password.
    in.locked = true;
    REQUIRE(classify_login_attempt(in) == LoginVerdict::Locked);
    REQUIRE(to_login_error(classify_login_attempt(in)).value()
            == LoginError::Locked);
}
