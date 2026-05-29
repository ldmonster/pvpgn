// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_token_issuer.hpp
/// Port: opaque session-token lifecycle (issue / validate / revoke).

#include <string>
#include <string_view>

#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Port: session token issuer interface for hexagonal architecture.
/// Implementations may use JWT, HMAC-signed blobs, or in-memory maps.
class ISessionTokenIssuer {
public:
    virtual ~ISessionTokenIssuer() = default;

    /// Issue a new opaque session token for the given account.
    /// The returned string is an opaque bearer token.
    virtual std::string
    issue(domain::AccountId account_id) = 0;

    /// Validate `token` and return the `AccountId` it was issued for.
    /// Returns an error if the token is unknown, expired, or revoked.
    virtual core::Result<domain::AccountId>
    validate(std::string_view token) = 0;

    /// Revoke `token` (logout). No-op if the token is already unknown.
    virtual void
    revoke(std::string_view token) noexcept = 0;
};

}  // namespace pvpgn::application::ports
