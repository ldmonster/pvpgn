// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::KickFromClan`.

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "application/social/kick_from_clan.hpp"
#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::KickFromClan;
using application::social::KickFromClanError;

// Helper: create a clan with chieftain + grunt member, save it.
// Returns the clan id.
domain::ClanId seed_clan_with_grunt(infra::inmemory::InMemoryClanRepository& repo,
                                    domain::AccountId chieftain,
                                    domain::AccountId grunt) {
    domain::ClanId id{300};
    auto clan_r = domain::social::Clan::create(id, "KCK", "Kick Clan",
                                               chieftain, domain::ClientTag{});
    REQUIRE(clan_r);
    clan_r.value().join(grunt, domain::social::ClanRank::Grunt);
    REQUIRE(repo.save(clan_r.value()));
    return id;
}

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryClanRepository> clans =
        std::make_shared<infra::inmemory::InMemoryClanRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId chieftain{1};
    domain::AccountId grunt{2};
    domain::AccountId outsider{99};

    KickFromClan make_uc() { return KickFromClan{clans, bus}; }
};

}  // namespace

TEST_CASE("KickFromClan: chieftain can kick a grunt",
          "[application][social][kick_from_clan]") {
    Fixture f;
    auto clan_id = seed_clan_with_grunt(*f.clans, f.chieftain, f.grunt);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.grunt);

    REQUIRE(r);
}

TEST_CASE("KickFromClan: unknown clan returns ClanNotFound",
          "[application][social][kick_from_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::ClanId{999}, f.chieftain, f.grunt);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromClanError::ClanNotFound);
}

TEST_CASE("KickFromClan: kicker not in clan returns KickerNotInClan",
          "[application][social][kick_from_clan]") {
    Fixture f;
    auto clan_id = seed_clan_with_grunt(*f.clans, f.chieftain, f.grunt);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.outsider, f.grunt);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromClanError::KickerNotInClan);
}

TEST_CASE("KickFromClan: peon kicker returns InsufficientRank",
          "[application][social][kick_from_clan]") {
    Fixture f;
    domain::AccountId peon{3};
    auto clan_id = seed_clan_with_grunt(*f.clans, f.chieftain, f.grunt);

    // Add peon
    auto clan_r = f.clans->find_by_id(clan_id);
    REQUIRE(clan_r);
    clan_r.value()->join(peon, domain::social::ClanRank::Peon);
    REQUIRE(f.clans->save(*clan_r.value()));

    auto uc = f.make_uc();
    auto r = uc.execute(clan_id, peon, f.grunt);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromClanError::InsufficientRank);
}

TEST_CASE("KickFromClan: target not in clan returns TargetNotMember",
          "[application][social][kick_from_clan]") {
    Fixture f;
    auto clan_id = seed_clan_with_grunt(*f.clans, f.chieftain, f.grunt);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.outsider);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromClanError::TargetNotMember);
}

TEST_CASE("KickFromClan: cannot kick chieftain returns CannotKickChieftain",
          "[application][social][kick_from_clan]") {
    Fixture f;
    auto clan_id = seed_clan_with_grunt(*f.clans, f.chieftain, f.grunt);
    auto uc = f.make_uc();

    // Grunt tries to kick chieftain (even if rank check passes, chieftain is protected)
    // Use chieftain kicking chieftain (self-kick) — chieftain rank is protected
    auto r = uc.execute(clan_id, f.chieftain, f.chieftain);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromClanError::CannotKickChieftain);
}
