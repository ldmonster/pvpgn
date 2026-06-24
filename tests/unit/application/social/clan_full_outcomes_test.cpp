// SPDX-License-Identifier: GPL-2.0-or-later
//
// Capacity ("clan full") outcome tests for the JoinClan and InviteToClan
// use-cases. These arms are unreachable from the base tests because seeding a
// clan to its 250-member cap requires `Clan::rehydrate` with a pre-built roster.
// Mirrors join_clan_test.cpp / invite_to_clan_test.cpp for fakes/helpers.

#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/social/invite_to_clan.hpp"
#include "application/social/join_clan.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::InviteToClan;
using application::social::InviteToClanError;
using application::social::JoinClan;
using application::social::JoinClanError;

// Build and persist a clan already at the 250-member cap. The member at index 0
// is the Chieftain (so authority-checked use-cases like invite can be driven).
domain::ClanId seed_full_clan(infra::inmemory::InMemoryClanRepository& repo,
                              domain::ClanId id,
                              domain::AccountId chieftain) {
    std::vector<domain::social::ClanMember> members;
    members.reserve(domain::social::Clan::kMaxMembers);
    members.push_back({chieftain, domain::social::ClanRank::Chieftain});
    // Fill the remaining slots with distinct grunts.
    for (std::uint32_t i = 1; i < domain::social::Clan::kMaxMembers; ++i) {
        members.push_back(
            {domain::AccountId{1000 + i}, domain::social::ClanRank::Grunt});
    }
    auto clan = domain::social::Clan::rehydrate(id, "FUL", "Full Clan",
                                                domain::ClientTag{},
                                                std::move(members));
    REQUIRE(clan.size() == domain::social::Clan::kMaxMembers);
    REQUIRE(clan.is_full());
    REQUIRE(repo.save(clan));
    return id;
}

}  // namespace

TEST_CASE("JoinClan: joining a clan at capacity returns ClanFull",
          "[application][social][join_clan]") {
    auto clans = std::make_shared<infra::inmemory::InMemoryClanRepository>();
    auto bus   = std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId chieftain{1};
    domain::AccountId newcomer{500000};
    auto clan_id = seed_full_clan(*clans, domain::ClanId{700}, chieftain);

    JoinClan uc{clans, bus};
    auto r = uc.execute(clan_id, newcomer);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinClanError::ClanFull);
}

TEST_CASE("InviteToClan: inviting into a clan at capacity returns ClanFull",
          "[application][social][invite_to_clan]") {
    auto clans = std::make_shared<infra::inmemory::InMemoryClanRepository>();
    auto bus   = std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId chieftain{1};
    domain::AccountId invitee{500000};
    auto clan_id = seed_full_clan(*clans, domain::ClanId{701}, chieftain);

    InviteToClan uc{clans, bus};
    // The chieftain is a valid (Shaman+) inviter, so the only failure left is
    // capacity — driving the aggregate's Full -> ClanFull mapping.
    auto r = uc.execute(clan_id, chieftain, invitee);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == InviteToClanError::ClanFull);
}
