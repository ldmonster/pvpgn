// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/invite_to_clan.hpp"

#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
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

    auto clan_ptr = clan_result.value();
    auto& clan = *clan_ptr;

    // 2. Delegate authorization + admission to the aggregate — the "inviter
    //    must be Shaman+" invariant (and membership/capacity) live in Clan.
    switch (clan.invite_member(inviter, invitee)) {
        case domain::social::Clan::InviteOutcome::InviterNotMember:
            return core::fail(InviteToClanError::InviterNotInClan);
        case domain::social::Clan::InviteOutcome::InsufficientRank:
            return core::fail(InviteToClanError::InsufficientRank);
        case domain::social::Clan::InviteOutcome::AlreadyMember:
            return core::fail(InviteToClanError::TargetAlreadyMember);
        case domain::social::Clan::InviteOutcome::Full:
            return core::fail(InviteToClanError::ClanFull);
        case domain::social::Clan::InviteOutcome::Invited:
            break;
    }

    // 3. Save updated clan
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(InviteToClanError::PersistenceFailed);
    }

    // 7. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, InviteToClanError>{};
}

}  // namespace pvpgn::application::social
