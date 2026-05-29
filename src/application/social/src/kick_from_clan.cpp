// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/kick_from_clan.hpp"

#include <algorithm>

#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
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

    // 2. Verify kicker is in clan and has sufficient rank
    const auto& members = clan.members();
    auto kicker_it = std::find_if(members.begin(), members.end(),
                                  [kicker](const domain::social::ClanMember& m) {
                                      return m.account.value() == kicker.value();
                                  });

    if (kicker_it == members.end()) {
        return core::fail(KickFromClanError::KickerNotInClan);
    }

    // Only Shaman+ can kick
    if (kicker_it->rank > domain::social::ClanRank::Shaman) {
        return core::fail(KickFromClanError::InsufficientRank);
    }

    // 3. Check if target is member
    auto target_it = std::find_if(members.begin(), members.end(),
                                  [target](const domain::social::ClanMember& m) {
                                      return m.account.value() == target.value();
                                  });

    if (target_it == members.end()) {
        return core::fail(KickFromClanError::TargetNotMember);
    }

    // 4. Cannot kick chieftain
    if (target_it->rank == domain::social::ClanRank::Chieftain) {
        return core::fail(KickFromClanError::CannotKickChieftain);
    }

    // 5. Remove member
    if (!clan.remove(target)) {
        return core::fail(KickFromClanError::TargetNotMember);
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
