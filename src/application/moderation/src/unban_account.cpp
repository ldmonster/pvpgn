// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/unban_account.hpp"

#include "application/ports/account_ban_repository.hpp"
#include "application/ports/event_bus.hpp"

namespace pvpgn::application::moderation {

core::Result<void, UnbanAccountError>
UnbanAccount::execute(domain::AccountId target, domain::AccountId by_admin) {
    // 1. Check if account is currently banned
    auto existing_ban = bans_->find_active_ban(target, std::chrono::system_clock::now());
    if (!existing_ban || !existing_ban.value()) {
        return core::fail(UnbanAccountError::NotBanned);
    }

    // 2. Remove the ban
    auto remove_result = bans_->remove_ban(target);
    if (!remove_result) {
        return core::fail(UnbanAccountError::PersistenceFailed);
    }

    // 3. Publish events (would include AccountUnbanned domain event)
    // Events would be published via event_bus_

    return core::Result<void, UnbanAccountError>();
}

}  // namespace pvpgn::application::moderation
