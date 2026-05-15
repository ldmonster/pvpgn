// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/ban_account.hpp"

#include "application/ports/account_ban_repository.hpp"
#include "application/ports/account_repository.hpp"
#include "application/ports/event_bus.hpp"

namespace pvpgn::application::moderation {

core::Result<void, BanAccountError>
BanAccount::execute(const BanAccountRequest& req) {
    // 1. Verify target account exists
    auto target_result = accounts_->find_by_id(req.target);
    if (!target_result) {
        return core::fail(BanAccountError::TargetNotFound);
    }

    // 2. Validate reason
    if (req.reason.empty()) {
        return core::fail(BanAccountError::InvalidReason);
    }

    // 3. Check if already banned
    auto existing_ban = bans_->find_active_ban(req.target, core::SystemTime::now());
    if (existing_ban && existing_ban.value()) {
        return core::fail(BanAccountError::AlreadyBanned);
    }

    // 4. Create and save ban entry
    application::ports::AccountBan ban{
        .banned_account = req.target,
        .banned_by = req.banned_by,
        .reason = req.reason,
        .banned_at = core::SystemTime::now(),
        .expires_at = req.expires_at,
    };

    auto save_result = bans_->add_ban(ban);
    if (!save_result) {
        return core::fail(BanAccountError::PersistenceFailed);
    }

    // 5. Publish events (would include AccountBanned domain event)
    // Events would be published via event_bus_

    return core::ok();
}

}  // namespace pvpgn::application::moderation
