// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests for application/email_management dispatchers (R170.a).

#include "application/email_management/email_change.hpp"
#include "application/email_management/password_recovery.hpp"

#include <catch2/catch_test_macros.hpp>

using pvpgn::application::email_management::dispatch_email_change;
using pvpgn::application::email_management::dispatch_password_recovery;
using pvpgn::application::email_management::EmailChangeRequest;
using pvpgn::application::email_management::EmailChangeResponse;
using pvpgn::application::email_management::EmailChangeStatus;
using pvpgn::application::email_management::PasswordRecoveryRequest;
using pvpgn::application::email_management::PasswordRecoveryStatus;

// ---------- email_change: set-when-unset ----------------------------------

TEST_CASE("email_change: set-when-unset accepts valid email",
          "[application][email_management][email_change]") {
    EmailChangeRequest req{};
    req.username        = "alice";
    req.stored_email    = "";
    req.new_email       = "alice@example.com";
    req.allow_when_unset = true;

    auto resp = dispatch_email_change(req);
    REQUIRE(resp.status == EmailChangeStatus::kAccepted);
    REQUIRE(resp.accepted_email == "alice@example.com");
}

TEST_CASE("email_change: set-when-unset rejected when disabled",
          "[application][email_management][email_change]") {
    EmailChangeRequest req{};
    req.stored_email    = "";
    req.new_email       = "alice@example.com";
    req.allow_when_unset = false;

    auto resp = dispatch_email_change(req);
    REQUIRE(resp.status == EmailChangeStatus::kRejected);
}

TEST_CASE("email_change: set-when-unset with non-empty current_email is mismatch",
          "[application][email_management][email_change]") {
    EmailChangeRequest req{};
    req.stored_email        = "";
    req.current_email_claim = "bogus@example.com";
    req.new_email           = "alice@example.com";
    req.allow_when_unset    = true;

    auto resp = dispatch_email_change(req);
    REQUIRE(resp.status == EmailChangeStatus::kCurrentMismatch);
}

TEST_CASE("email_change: set-when-unset rejects invalid new_email",
          "[application][email_management][email_change]") {
    EmailChangeRequest req{};
    req.stored_email     = "";
    req.allow_when_unset = true;

    SECTION("empty") {
        req.new_email = "";
        REQUIRE(dispatch_email_change(req).status ==
                EmailChangeStatus::kNewInvalid);
    }
    SECTION("no @") {
        req.new_email = "alice";
        REQUIRE(dispatch_email_change(req).status ==
                EmailChangeStatus::kNewInvalid);
    }
    SECTION("two @") {
        req.new_email = "a@b@c.com";
        REQUIRE(dispatch_email_change(req).status ==
                EmailChangeStatus::kNewInvalid);
    }
    SECTION("empty local part") {
        req.new_email = "@example.com";
        REQUIRE(dispatch_email_change(req).status ==
                EmailChangeStatus::kNewInvalid);
    }
    SECTION("no domain dot") {
        req.new_email = "alice@example";
        REQUIRE(dispatch_email_change(req).status ==
                EmailChangeStatus::kNewInvalid);
    }
}

// ---------- email_change: replace path ------------------------------------

TEST_CASE("email_change: replace accepts when current matches (case-insensitive)",
          "[application][email_management][email_change]") {
    EmailChangeRequest req{};
    req.stored_email        = "Alice@Example.COM";
    req.current_email_claim = "alice@example.com";
    req.new_email           = "alice2@example.com";
    req.allow_when_unset    = true;

    auto resp = dispatch_email_change(req);
    REQUIRE(resp.status == EmailChangeStatus::kAccepted);
    REQUIRE(resp.accepted_email == "alice2@example.com");
}

TEST_CASE("email_change: replace rejects mismatching current",
          "[application][email_management][email_change]") {
    EmailChangeRequest req{};
    req.stored_email        = "alice@example.com";
    req.current_email_claim = "other@example.com";
    req.new_email           = "alice2@example.com";

    REQUIRE(dispatch_email_change(req).status ==
            EmailChangeStatus::kCurrentMismatch);
}

TEST_CASE("email_change: replace rejects invalid new_email even with matching current",
          "[application][email_management][email_change]") {
    EmailChangeRequest req{};
    req.stored_email        = "alice@example.com";
    req.current_email_claim = "alice@example.com";
    req.new_email           = "garbage";

    REQUIRE(dispatch_email_change(req).status ==
            EmailChangeStatus::kNewInvalid);
}

// ---------- password_recovery ---------------------------------------------

TEST_CASE("password_recovery: disabled returns kDisabled",
          "[application][email_management][password_recovery]") {
    PasswordRecoveryRequest req{};
    req.username        = "alice";
    req.stored_email    = "alice@example.com";
    req.claimed_email   = "alice@example.com";
    req.feature_enabled = false;

    REQUIRE(dispatch_password_recovery(req).status ==
            PasswordRecoveryStatus::kDisabled);
}

TEST_CASE("password_recovery: no stored email is kRejected",
          "[application][email_management][password_recovery]") {
    PasswordRecoveryRequest req{};
    req.stored_email    = "";
    req.claimed_email   = "alice@example.com";
    req.feature_enabled = true;

    REQUIRE(dispatch_password_recovery(req).status ==
            PasswordRecoveryStatus::kRejected);
}

TEST_CASE("password_recovery: mismatch is kRejected (indistinguishable from no-account)",
          "[application][email_management][password_recovery]") {
    PasswordRecoveryRequest req{};
    req.stored_email    = "alice@example.com";
    req.claimed_email   = "other@example.com";
    req.feature_enabled = true;

    REQUIRE(dispatch_password_recovery(req).status ==
            PasswordRecoveryStatus::kRejected);
}

TEST_CASE("password_recovery: case-insensitive match is kAccepted",
          "[application][email_management][password_recovery]") {
    PasswordRecoveryRequest req{};
    req.stored_email    = "Alice@Example.COM";
    req.claimed_email   = "alice@example.com";
    req.feature_enabled = true;

    auto resp = dispatch_password_recovery(req);
    REQUIRE(resp.status == PasswordRecoveryStatus::kAccepted);
    REQUIRE(resp.token_to_deliver.empty());  // caller-side fill
}
