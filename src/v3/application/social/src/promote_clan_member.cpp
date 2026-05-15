// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/promote_clan_member.hpp"

#include <algorithm>
#include <string>

#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<void, PromoteClanMemberError>
PromoteClanMember::execute(domain::ClanId clan_id, domain::AccountId promoter,
                           domain::AccountId target, std::string_view new_rank) {
    // 1. Find the clan
    auto clan_result = clans_->find_by_id(clan_id);
    if (!clan_result) {
        return core::fail(PromoteClanMemberError::ClanNotFound);
    }

    domain::social::Clan clan = clan_result.value();

    // 2. Verify promoter is chieftain
    const auto& members = clan.members();
    auto promoter_it = std::find_if(
        members.begin(), members.end(),
        [promoter](const domain::social::ClanMember& m) {
            return m.account == promoter && m.rank == domain::social::ClanRank::Chieftain;
        });

    if (promoter_it == members.end()) {
        return core::fail(PromoteClanMemberError::InsufficientRank);
    }

    // 3. Check if target is member
    if (!clan.contains(target)) {
        return core::fail(PromoteClanMemberError::TargetNotMember);
    }

    // 4. Parse new rank
    domain::social::ClanRank rank;
    if (new_rank == "peon") {
        rank = domain::social::ClanRank::Peon;
    } else if (new_rank == "grunt") {
        rank = domain::social::ClanRank::Grunt;
    } else if (new_rank == "shaman") {
        rank = domain::social::ClanRank::Shaman;
    } else if (new_rank == "chieftain") {
        rank = domain::social::ClanRank::Chieftain;
    } else {
        return core::fail(PromoteClanMemberError::InvalidRank);
    }

    // 5. Update rank
    if (!clan.set_rank(target, rank)) {
        return core::fail(PromoteClanMemberError::TargetNotMember);
    }

    // 6. Save updated clan
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(PromoteClanMemberError::PersistenceFailed);
    }

    // 7. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::ok();
}

}  // namespace pvpgn::application::social
