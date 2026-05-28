// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/list_bans.hpp"

#include "application/ports/account_ban_repository.hpp"
#include "application/ports/ip_ban_repository.hpp"

namespace pvpgn::application::moderation {

core::Result<std::vector<BanRecord>>
ListBans::execute(const ListBansQuery& query) const {
    std::vector<BanRecord> results;

    // 1. Collect account bans (if filter is Account or nullopt)
    if (!query.filter_type.has_value() ||
        query.filter_type.value() == BanType::Account) {
        account_bans_->for_each([&](const application::ports::AccountBan& ban) {
            if (results.size() >= query.max_results) {
                return false;  // Stop early
            }
            results.push_back(BanRecord{
                .type      = BanType::Account,
                .target    = std::to_string(ban.banned_account.value()),
                .reason    = ban.reason,
                .banned_by = ban.banned_by,
                .banned_at = ban.banned_at,
                .expires_at = ban.expires_at,
            });
            return true;
        });
    }

    // 2. Collect IP bans (if filter is Ip or nullopt)
    if (!query.filter_type.has_value() ||
        query.filter_type.value() == BanType::Ip) {
        ip_bans_->for_each_entry([&](const domain::moderation::IpBanEntry& entry) {
            if (results.size() >= query.max_results) {
                return false;  // Stop early
            }
            results.push_back(BanRecord{
                .type      = BanType::Ip,
                .target    = entry.ip.to_string(),
                .reason    = entry.reason,
                .banned_by = entry.issuer,
                .banned_at = entry.issued_at,
                .expires_at = entry.expires_at,
            });
            return true;
        });
    }

    return results;
}

}  // namespace pvpgn::application::moderation
