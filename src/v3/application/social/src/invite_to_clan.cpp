// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/invite_to_clan.hpp"

#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<void, InviteToClanError>
InviteToClan::execute(domain::ClanId clan_id, domain::AccountId inviter,
                      domain::AccountId invitee) {
    // 1. Find the clan
    auto clan_result = clans_->find_by_id(clan_id);
    if (!clan_result) {
        return core::fail(InviteToClanError::ClanNotFound);
    }

    domain::social::Clan clan = clan_result.value();

    // 2. Verify inviter is in clan and has sufficient rank
    const auto& members = clan.members();
    auto inviter_it = std::find_if(members.begin(), members.end(),
                                   [inviter](const domain::social::ClanMember& m) {
                                       return m.account == inviter;
                                   });

    if (inviter_it == members.end()) {
        return core::fail(InviteToClanError::InviterNotInClan);
    }

    // Only Shaman+ can invite
    if (inviter_it->rank > domain::social::ClanRank::Shaman) {
        return core::fail(InviteToClanError::InsufficientRank);
    }

    // 3. Check if target is already member
    if (clan.contains(invitee)) {
        return core::fail(InviteToClanError::TargetAlreadyMember);
    }

    // 4. Check if clan is full
    if (clan.is_full()) {
        return core::fail(InviteToClanError::ClanFull);
    }

    // 5. Add member as Peon (invited/probation)
    auto join_outcome = clan.join(invitee, domain::social::ClanRank::Peon);
    if (join_outcome == domain::social::Clan::JoinOutcome::Full) {
        return core::fail(InviteToClanError::ClanFull);
    }

    // 6. Save updated clan
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(InviteToClanError::PersistenceFailed);
    }

    // 7. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::ok();
}

}  // namespace pvpgn::application::social
