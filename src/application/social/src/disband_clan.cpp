// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/disband_clan.hpp"

#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<void, DisbandClanError>
DisbandClan::execute(domain::ClanId clan_id, domain::AccountId by_chieftain) {
    // 1. Find the clan
    auto clan_result = clans_->find_by_id(clan_id);
    if (!clan_result) {
        return core::fail(DisbandClanError::ClanNotFound);
    }

    auto clan_ptr = clan_result.value();
    auto& clan = *clan_ptr;

    // 2. Verify caller is chieftain
    const auto& members = clan.members();
    auto chieftain_it = std::find_if(
        members.begin(), members.end(),
        [by_chieftain](const domain::social::ClanMember& m) {
            return m.account.value() == by_chieftain.value() && m.rank == domain::social::ClanRank::Chieftain;
        });

    if (chieftain_it == members.end()) {
        return core::fail(DisbandClanError::NotChieftain);
    }

    // 3. Remove the clan
    auto remove_result = clans_->remove(clan.tag());
    if (!remove_result) {
        return core::fail(DisbandClanError::PersistenceFailed);
    }

    // 4. Publish disbanding event would go here (would need domain event support)
    // For now, events would be published via event_bus

    return core::Result<void, DisbandClanError>{};
}

}  // namespace pvpgn::application::social
