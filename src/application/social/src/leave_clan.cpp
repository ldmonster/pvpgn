// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/leave_clan.hpp"

#include <algorithm>

#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
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

    // 2. Verify account is a member
    const auto& members = clan.members();
    auto member_it = std::find_if(members.begin(), members.end(),
                                  [account_id](const domain::social::ClanMember& m) {
                                      return m.account == account_id;
                                  });

    if (member_it == members.end()) {
        return core::fail(LeaveClanError::NotAMember);
    }

    // 3. Chieftain must disband instead of leaving
    if (member_it->rank == domain::social::ClanRank::Chieftain) {
        return core::fail(LeaveClanError::LeaderMustDisband);
    }

    // 4. Remove the member
    if (!clan.remove(account_id)) {
        return core::fail(LeaveClanError::NotAMember);
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
