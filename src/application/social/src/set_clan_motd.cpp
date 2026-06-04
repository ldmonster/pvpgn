// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/set_clan_motd.hpp"

#include <algorithm>

#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
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

    // 2. Delegate authorization, the length rule, and the MOTD state to the
    //    aggregate — "setter must be Shaman+" and the length limit are clan
    //    invariants, and the MOTD now lives in Clan (previously a no-op stub).
    switch (clan.set_motd(setter, motd)) {
        case domain::social::Clan::MotdOutcome::SetterNotMember:
            return core::fail(SetClanMotdError::SetterNotInClan);
        case domain::social::Clan::MotdOutcome::InsufficientRank:
            return core::fail(SetClanMotdError::InsufficientRank);
        case domain::social::Clan::MotdOutcome::TooLong:
            return core::fail(SetClanMotdError::MotdTooLong);
        case domain::social::Clan::MotdOutcome::Set:
            break;
    }

    // 3. Save updated clan
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
