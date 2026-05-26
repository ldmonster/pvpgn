// SPDX-License-Identifier: GPL-2.0-or-later
//
// application/email_management/email_change.hpp -- R169.d skeleton
//
// Interface-only header for the email-change handler family. The
// legacy bnetd `handle_changeemail` / `handle_setemail` /
// `handle_getpassword` paths are slated to be stranglered through
// this module in R170+. No implementation is provided in this
// round -- this header only commits to the request / response /
// status shapes so that callers (and tests) can be wired ahead of
// the implementation.
//
// Layering: application MUST NOT depend on infra. The dispatcher
// receives a fully-resolved snapshot of the account state via the
// request struct (no I/O happens inside the dispatcher).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pvpgn::application::email_management {

/// Outcome of an email-change request.
enum class EmailChangeStatus : std::uint8_t {
    /// New address accepted and should be persisted by the caller.
    kAccepted = 0,
    /// The supplied current address did not match the stored one.
    kCurrentMismatch = 1,
    /// The new address is syntactically invalid.
    kNewInvalid = 2,
    /// The account does not currently have an email address set
    /// (set-email path) or the operation is disabled by config.
    kRejected = 3,
};

/// Input to `dispatch_email_change`.
///
/// All fields are caller-owned views; the dispatcher copies what it
/// needs into the response. The `stored_email` view may be empty
/// when the account has no address set (set-email mode).
struct EmailChangeRequest {
    std::string_view username;
    std::string_view stored_email;
    std::string_view current_email_claim;
    std::string_view new_email;
    bool             allow_when_unset = true;
};

/// Output of `dispatch_email_change`.
struct EmailChangeResponse {
    EmailChangeStatus status         = EmailChangeStatus::kRejected;
    std::string       accepted_email;  // populated only on kAccepted
};

/// Pure-function dispatch -- no I/O, no global state.
///
/// NOTE: R169.d ships the declaration only. The implementation
/// lands in R170 alongside the legacy `handle_changeemail` /
/// `handle_setemail` strangler bridges.
EmailChangeResponse dispatch_email_change(EmailChangeRequest const& req);

}  // namespace pvpgn::application::email_management
