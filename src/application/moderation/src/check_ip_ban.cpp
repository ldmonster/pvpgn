// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/check_ip_ban.hpp"

#include "application/ports/ip_ban_repository.hpp"

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

    // 3. Search for the matching ban entry to get reason and expiration
    CheckIpBanResult result{
        .banned = true,
        .reason = "Banned",
        .expires_at = std::nullopt,
    };

    // 4. Iterate over entries to find matching IP
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
