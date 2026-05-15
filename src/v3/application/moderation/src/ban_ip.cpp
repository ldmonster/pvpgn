// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/ban_ip.hpp"

#include "application/ports/event_bus.hpp"
#include "application/ports/ip_ban_repository.hpp"

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
    auto existing_ban = bans_->find_ban(req.target);
    if (existing_ban) {
        return core::fail(BanIpError::AlreadyBanned);
    }

    // 4. Create and save IP ban entry
    application::ports::IpBan ban{
        .ip = req.target,
        .banned_by = req.banned_by,
        .reason = req.reason,
        .is_cidr_range = req.is_cidr_range,
        .banned_at = core::SystemTime::now(),
        .expires_at = req.expires_at,
    };

    auto save_result = bans_->add_ban(ban);
    if (!save_result) {
        return core::fail(BanIpError::PersistenceFailed);
    }

    // 5. Publish events (would include IpBanned domain event)
    // Events would be published via event_bus_

    return core::ok();
}

}  // namespace pvpgn::application::moderation
