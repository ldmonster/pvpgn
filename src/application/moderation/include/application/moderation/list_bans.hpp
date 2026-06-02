// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file list_bans.hpp
/// LIST_BANS use-case — enumerate active account and/or IP bans.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/moderation/ports.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::moderation {

enum class BanType : std::uint8_t {
    Account,
    Ip,
};

struct BanRecord {
    BanType          type;
    std::string      target;       ///< account name or IP string
    std::string      reason;
    domain::AccountId banned_by;   ///< admin account ID
    core::SystemTime  banned_at;
    std::optional<core::SystemTime> expires_at; ///< nullopt = permanent
};

struct ListBansQuery {
    std::optional<BanType> filter_type; ///< nullopt = all types
    std::uint32_t          max_results = 100;
};

class ListBans {
public:
    ListBans(std::shared_ptr<domain::moderation::IAccountBanRepository> account_bans,
             std::shared_ptr<domain::moderation::IIpBanRepository> ip_bans)
        : account_bans_(account_bans), ip_bans_(ip_bans) {}

    [[nodiscard]] core::Result<std::vector<BanRecord>>
    execute(const ListBansQuery& query) const;

private:
    std::shared_ptr<domain::moderation::IAccountBanRepository> account_bans_;
    std::shared_ptr<domain::moderation::IIpBanRepository>      ip_bans_;
};

}  // namespace pvpgn::application::moderation
