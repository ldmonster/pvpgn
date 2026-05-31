// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_token_issuer.hpp
/// Port for opaque session-token issuance + validation.

#include <string>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

class ISessionTokenIssuer {
public:
    virtual ~ISessionTokenIssuer() = default;

    /// Issue a fresh opaque session token bound to `account_id`.
    virtual std::string issue(domain::AccountId account_id) = 0;

    /// Look up the account that owns `token`. Returns NotFound if the
    /// token is unknown or has been revoked.
    [[nodiscard]] virtual core::Result<domain::AccountId>
        validate(std::string_view token) = 0;

    /// Revoke `token`. No-op if the token is unknown.
    virtual void revoke(std::string_view token) noexcept = 0;
};

} // namespace pvpgn::application::ports
