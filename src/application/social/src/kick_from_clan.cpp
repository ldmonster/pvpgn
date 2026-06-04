// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/kick_from_clan.hpp"

#include <algorithm>

#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<void, KickFromClanError>
KickFromClan::execute(domain::ClanId clan_id, domain::AccountId kicker,
                      domain::AccountId target) {
    // 1. Find the clan
    auto clan_result = clans_->find_by_id(clan_id);
    if (!clan_result) {
        return core::fail(KickFromClanError::ClanNotFound);
    }

    auto clan_ptr = clan_result.value();
    auto& clan = *clan_ptr;

    // 2. Delegate the authorization + removal rules to the aggregate — the
    //    "kicker must be Shaman+" and "the Chieftain cannot be kicked"
    //    invariants live in Clan, not here.
    switch (clan.kick_member(kicker, target)) {
        case domain::social::Clan::KickOutcome::KickerNotMember:
            return core::fail(KickFromClanError::KickerNotInClan);
        case domain::social::Clan::KickOutcome::InsufficientRank:
            return core::fail(KickFromClanError::InsufficientRank);
        case domain::social::Clan::KickOutcome::TargetNotMember:
            return core::fail(KickFromClanError::TargetNotMember);
        case domain::social::Clan::KickOutcome::CannotKickChieftain:
            return core::fail(KickFromClanError::CannotKickChieftain);
        case domain::social::Clan::KickOutcome::Kicked:
            break;
    }

    // 6. Save updated clan
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(KickFromClanError::PersistenceFailed);
    }

    // 7. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, KickFromClanError>{};
}

}  // namespace pvpgn::application::social
