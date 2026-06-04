// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/leave_clan.hpp"

#include <algorithm>

#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<void, LeaveClanError>
LeaveClan::execute(domain::ClanId clan_id, domain::AccountId account_id) {
    // 1. Find the clan
    auto clan_result = clans_->find_by_id(clan_id);
    if (!clan_result) {
        return core::fail(LeaveClanError::ClanNotFound);
    }

    auto clan_ptr = clan_result.value();
    auto& clan = *clan_ptr;

    // 2. Delegate to the aggregate — "must be a member" and "the Chieftain must
    //    disband rather than leave" are clan invariants.
    switch (clan.leave(account_id)) {
        case domain::social::Clan::LeaveOutcome::NotMember:
            return core::fail(LeaveClanError::NotAMember);
        case domain::social::Clan::LeaveOutcome::ChieftainMustDisband:
            return core::fail(LeaveClanError::LeaderMustDisband);
        case domain::social::Clan::LeaveOutcome::Left:
            break;
    }

    // 5. Save updated clan
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(LeaveClanError::PersistenceFailed);
    }

    // 6. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, LeaveClanError>{};
}

}  // namespace pvpgn::application::social
