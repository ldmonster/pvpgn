// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/ban_ip.hpp"

#include "domain/shared/event_bus.hpp"
#include "domain/moderation/ports.hpp"

namespace pvpgn::application::moderation {

core::Result<void, BanIpError>
BanIp::execute(const BanIpRequest& req) {
    // 1. Validate IP address
    // (IP validation would be done by IpAddress type)

    // 2. Validate reason
    if (req.reason.empty()) {
        return core::fail(BanIpError::InvalidReason);
    }

    // 3. Check if already banned
    auto is_banned = bans_->is_banned(req.target);
    if (!is_banned) {
        return core::fail(BanIpError::PersistenceFailed);
    }
    if (is_banned.value()) {
        return core::fail(BanIpError::AlreadyBanned);
    }

    // 4. Create and save IP ban entry
    domain::moderation::IpBanEntry ban{
        .ip = req.target,
        .reason = req.reason,
        .issuer = req.banned_by,
        .issued_at = std::chrono::system_clock::now(),
        .expires_at = req.expires_at,
    };

    auto save_result = bans_->add_ban(ban);
    if (!save_result) {
        return core::fail(BanIpError::PersistenceFailed);
    }

    // 5. Publish events (would include IpBanned domain event)
    // Events would be published via event_bus_

    return core::Result<void, BanIpError>{};
}

}  // namespace pvpgn::application::moderation
