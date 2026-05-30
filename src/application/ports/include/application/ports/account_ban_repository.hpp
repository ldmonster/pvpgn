// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_ban_repository.hpp
/// Application-layer port for per-account bans.

#include <functional>
#include <optional>
#include <string>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Represents a ban on a specific account.
struct AccountBan {
    domain::AccountId                 banned_account{};
    domain::AccountId                 banned_by{};
    std::string                       reason;
    core::SystemTime                  banned_at{};
    std::optional<core::SystemTime>   expires_at;

    /// True if the ban is in force at @p now (no expiry, or expiry in future).
    [[nodiscard]] bool active_at(core::SystemTime now) const noexcept {
        return !expires_at.has_value() || *expires_at > now;
    }
};

class IAccountBanRepository {
public:
    virtual ~IAccountBanRepository() = default;

    IAccountBanRepository(const IAccountBanRepository&)            = delete;
    IAccountBanRepository& operator=(const IAccountBanRepository&) = delete;
    IAccountBanRepository(IAccountBanRepository&&)                 = delete;
    IAccountBanRepository& operator=(IAccountBanRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<std::optional<AccountBan>>
    find_active_ban(domain::AccountId account_id,
                    core::SystemTime now) const = 0;

    virtual core::Status<> add_ban(const AccountBan& ban) = 0;

    virtual core::Status<> remove_ban(domain::AccountId account_id) = 0;

    virtual void for_each(
        std::function<bool(const AccountBan&)> predicate) const = 0;

protected:
    IAccountBanRepository() = default;
};

} // namespace pvpgn::application::ports
