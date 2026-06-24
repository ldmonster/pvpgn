// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/check_ip_ban.hpp"

#include <chrono>

#include "domain/moderation/ports.hpp"

namespace pvpgn::application::moderation {

core::Result<CheckIpBanResult>
CheckIpBan::execute(const domain::IpAddress& ip) const {
    // 1. Check if the IP is banned
    auto is_banned_result = ban_repo_.is_banned(ip);
    if (!is_banned_result) {
        // Repository error — propagate it
        return core::fail(is_banned_result.error());
    }

    bool banned = is_banned_result.value();

    // 2. If not banned, return early
    if (!banned) {
        return CheckIpBanResult{
            .banned = false,
            .reason = "",
            .expires_at = std::nullopt,
        };
    }

    // 3. Recover the matching entry's reason / expiry. A range, wildcard or
    //    inclusive-range ban will NOT show up in the exact-entry list, so we
    //    ask the aggregate for the matching reason/expiry across *all* forms
    //    (exact, CIDR, wildcard, range) rather than scanning exact entries
    //    only (which mis-reported range matches as a permanent "Banned").
    CheckIpBanResult result{
        .banned = true,
        .reason = "Banned",
        .expires_at = std::nullopt,
    };

    if (auto banlist = ban_repo_.load_banlist()) {
        const auto now = std::chrono::system_clock::now();
        if (auto info = banlist.value().match_info(ip, now)) {
            result.reason = info->reason;
            result.expires_at = info->expires_at;
            return result;
        }
    }

    // Fallback (e.g. a repository that cannot snapshot its banlist): scan the
    // exact entries for a direct hit, preserving the original behaviour.
    ban_repo_.for_each_entry([&](const domain::moderation::IpBanEntry& entry) {
        if (entry.ip == ip) {
            result.reason = entry.reason;
            result.expires_at = entry.expires_at;
            return false;  // Found, break early
        }
        return true;  // Continue searching
    });

    return result;
}

}  // namespace pvpgn::application::moderation
