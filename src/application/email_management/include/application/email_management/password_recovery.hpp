// SPDX-License-Identifier: GPL-2.0-or-later
//
// Interface-only header for the `handle_getpassword` family. This
// covers the "I forgot my password, mail it to me" path. The
// dispatcher decides whether the request is acceptable, but the
// actual delivery (SMTP, audit log, etc.) is performed by infra
// adapters that the caller wires up.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pvpgn::application::email_management {

enum class PasswordRecoveryStatus : std::uint8_t {
    /// Request accepted -- caller should now deliver the password
    /// via the configured channel.
    kAccepted = 0,
    /// No such account, or the stored email does not match the
    /// claimed one. From the caller's perspective these are
    /// indistinguishable on purpose (enumeration guard).
    kRejected = 1,
    /// The recovery feature is disabled in config.
    kDisabled = 2,
};

struct PasswordRecoveryRequest {
    std::string_view username;
    std::string_view stored_email;
    std::string_view claimed_email;
    bool             feature_enabled = true;
};

struct PasswordRecoveryResponse {
    PasswordRecoveryStatus status = PasswordRecoveryStatus::kRejected;
    /// Populated on kAccepted; the password the caller should send.
    /// This is the *plaintext* token that the caller deals with
    /// (the dispatcher does not know whether the account stores
    /// hashed or plaintext passwords -- the caller supplies the
    /// already-resolved value separately).
    std::string token_to_deliver;
};

PasswordRecoveryResponse dispatch_password_recovery(
    PasswordRecoveryRequest const& req);

}  // namespace pvpgn::application::email_management
