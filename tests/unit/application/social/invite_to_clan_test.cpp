// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::InviteToClan`.

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "application/social/invite_to_clan.hpp"
#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::InviteToClan;
using application::social::InviteToClanError;

// Helper: create a clan with a chieftain and save it.
domain::ClanId seed_clan(infra::inmemory::InMemoryClanRepository& repo,
                         domain::AccountId chieftain) {
    domain::ClanId id{200};
    auto clan_r = domain::social::Clan::create(id, "INV", "Invite Clan",
                                               chieftain, domain::ClientTag{});
    REQUIRE(clan_r);
    REQUIRE(repo.save(clan_r.value()));
    return id;
}

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryClanRepository> clans =
        std::make_shared<infra::inmemory::InMemoryClanRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId chieftain{1};
    domain::AccountId invitee{2};
    domain::AccountId outsider{99};

    InviteToClan make_uc() { return InviteToClan{clans, bus}; }
};

}  // namespace

TEST_CASE("InviteToClan: chieftain can invite a new member",
          "[application][social][invite_to_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.invitee);

    REQUIRE(r);
}

TEST_CASE("InviteToClan: unknown clan returns ClanNotFound",
          "[application][social][invite_to_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::ClanId{999}, f.chieftain, f.invitee);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == InviteToClanError::ClanNotFound);
}

TEST_CASE("InviteToClan: inviter not in clan returns InviterNotInClan",
          "[application][social][invite_to_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.outsider, f.invitee);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == InviteToClanError::InviterNotInClan);
}

TEST_CASE("InviteToClan: peon inviter returns InsufficientRank",
          "[application][social][invite_to_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);

    // Add a peon member
    domain::AccountId peon{3};
    auto clan_r = f.clans->find_by_id(clan_id);
    REQUIRE(clan_r);
    clan_r.value()->join(peon, domain::social::ClanRank::Peon);
    REQUIRE(f.clans->save(*clan_r.value()));

    auto uc = f.make_uc();
    auto r = uc.execute(clan_id, peon, f.invitee);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == InviteToClanError::InsufficientRank);
}

TEST_CASE("InviteToClan: inviting existing member returns TargetAlreadyMember",
          "[application][social][invite_to_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);

    // Add invitee first
    auto clan_r = f.clans->find_by_id(clan_id);
    REQUIRE(clan_r);
    clan_r.value()->join(f.invitee, domain::social::ClanRank::Grunt);
    REQUIRE(f.clans->save(*clan_r.value()));

    auto uc = f.make_uc();
    auto r = uc.execute(clan_id, f.chieftain, f.invitee);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == InviteToClanError::TargetAlreadyMember);
}
