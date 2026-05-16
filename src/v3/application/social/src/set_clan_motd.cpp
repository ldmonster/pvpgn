// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/set_clan_motd.hpp"

#include <algorithm>

#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<void, SetClanMotdError>
SetClanMotd::execute(domain::ClanId clan_id, domain::AccountId setter,
                     std::string_view motd) {
    // 1. Find the clan
    auto clan_result = clans_->find_by_id(clan_id);
    if (!clan_result) {
        return core::fail(SetClanMotdError::ClanNotFound);
    }

    auto& clan = *clan_result.value();

    // 2. Verify setter is in clan and has sufficient rank
    const auto& members = clan.members();
    auto setter_it = std::find_if(members.begin(), members.end(),
                                  [setter](const domain::social::ClanMember& m) {
                                      return m.account.value() == setter.value();
                                  });

    if (setter_it == members.end()) {
        return core::fail(SetClanMotdError::SetterNotInClan);
    }

    // Only Shaman+ can set MOTD
    if (setter_it->rank > domain::social::ClanRank::Shaman) {
        return core::fail(SetClanMotdError::InsufficientRank);
    }

    // 3. Validate MOTD length (legacy limit ~256 chars)
    if (motd.size() > 256) {
        return core::fail(SetClanMotdError::MotdTooLong);
    }

    // 4. Update MOTD (would need to add to Clan aggregate)
    // For now, this is a placeholder pending Clan aggregate extension
    // clan.set_motd(motd);

    // 5. Save updated clan
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(SetClanMotdError::PersistenceFailed);
    }

    // 6. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, SetClanMotdError>{};
}

}  // namespace pvpgn::application::social
