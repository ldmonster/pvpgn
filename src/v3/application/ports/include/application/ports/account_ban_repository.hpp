// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_ban_repository.hpp
/// Persistence port for account-level bans (distinct from IP bans).
///
/// An account ban prevents a user from logging in with a specific account,
/// even if they connect from different IP addresses. Bans can be temporary
/// (with expiration) or permanent.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

struct AccountBan {
    domain::AccountId   banned_account;
    domain::AccountId   banned_by;
    std::string         reason;
    core::SystemTime    banned_at;
    std::optional<core::SystemTime> expires_at;

    /// Check if this ban is active at the given time.
    bool active_at(core::SystemTime now) const noexcept {
        if (expires_at && now >= *expires_at) {
            return false;  // Expired
        }
        return true;
    }
};

class IAccountBanRepository {
public:
    virtual ~IAccountBanRepository() = default;

    /// Find the active ban for an account at the current time.
    /// Returns empty Optional if no active ban exists.
    virtual core::Result<std::optional<AccountBan>>
    find_active_ban(domain::AccountId account_id, core::SystemTime now) const = 0;

    /// Add or update a ban entry.
    virtual core::Status<>
    add_ban(const AccountBan& ban) = 0;

    /// Remove an account ban.
    virtual core::Status<>
    remove_ban(domain::AccountId account_id) = 0;

    /// Iterate over all ban entries, applying predicate. Early exit on
    /// predicate returning false.
    virtual void
    for_each(std::function<bool(const AccountBan&)> predicate) const = 0;
};

}  // namespace pvpgn::application::ports
