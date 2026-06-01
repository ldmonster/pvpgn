// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/join_clan.hpp"

#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<void, JoinClanError>
JoinClan::execute(domain::ClanId clan_id, domain::AccountId account_id) {
    // 1. Find the clan
    auto clan_result = clans_->find_by_id(clan_id);
    if (!clan_result) {
        return core::fail(JoinClanError::ClanNotFound);
    }

    auto clan_ptr = clan_result.value();
    auto& clan = *clan_ptr;

    // 2. Attempt to join
    auto outcome = clan.join(account_id, domain::social::ClanRank::Peon);
    switch (outcome) {
        case domain::social::Clan::JoinOutcome::Joined:
            break;
        case domain::social::Clan::JoinOutcome::AlreadyMember:
            return core::fail(JoinClanError::AlreadyInClan);
        case domain::social::Clan::JoinOutcome::Full:
            return core::fail(JoinClanError::ClanFull);
    }

    // 3. Save updated clan
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(JoinClanError::PersistenceFailed);
    }

    // 4. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, JoinClanError>{};
}

}  // namespace pvpgn::application::social
