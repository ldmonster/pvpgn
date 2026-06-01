// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::SetClanMotd`.

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "application/social/set_clan_motd.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::SetClanMotd;
using application::social::SetClanMotdError;

// Helper: create a clan with chieftain + shaman, save it.
domain::ClanId seed_clan(infra::inmemory::InMemoryClanRepository& repo,
                         domain::AccountId chieftain,
                         domain::AccountId shaman) {
    domain::ClanId id{500};
    auto clan_r = domain::social::Clan::create(id, "MOT", "Motd Clan",
                                               chieftain, domain::ClientTag{});
    REQUIRE(clan_r);
    clan_r.value().join(shaman, domain::social::ClanRank::Shaman);
    REQUIRE(repo.save(clan_r.value()));
    return id;
}

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryClanRepository> clans =
        std::make_shared<infra::inmemory::InMemoryClanRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId chieftain{1};
    domain::AccountId shaman{2};
    domain::AccountId peon{3};
    domain::AccountId outsider{99};

    SetClanMotd make_uc() { return SetClanMotd{clans, bus}; }
};

}  // namespace

TEST_CASE("SetClanMotd: chieftain can set the MOTD",
          "[application][social][set_clan_motd]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.shaman);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, "Welcome to the clan!");

    REQUIRE(r);
}

TEST_CASE("SetClanMotd: shaman can set the MOTD",
          "[application][social][set_clan_motd]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.shaman);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.shaman, "Shaman's message");

    REQUIRE(r);
}

TEST_CASE("SetClanMotd: unknown clan returns ClanNotFound",
          "[application][social][set_clan_motd]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::ClanId{999}, f.chieftain, "Hello");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SetClanMotdError::ClanNotFound);
}

TEST_CASE("SetClanMotd: setter not in clan returns SetterNotInClan",
          "[application][social][set_clan_motd]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.shaman);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.outsider, "Hello");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SetClanMotdError::SetterNotInClan);
}

TEST_CASE("SetClanMotd: peon setter returns InsufficientRank",
          "[application][social][set_clan_motd]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.shaman);

    // Add peon
    auto clan_r = f.clans->find_by_id(clan_id);
    REQUIRE(clan_r);
    clan_r.value()->join(f.peon, domain::social::ClanRank::Peon);
    REQUIRE(f.clans->save(*clan_r.value()));

    auto uc = f.make_uc();
    auto r = uc.execute(clan_id, f.peon, "Hello");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SetClanMotdError::InsufficientRank);
}

TEST_CASE("SetClanMotd: MOTD longer than 256 chars returns MotdTooLong",
          "[application][social][set_clan_motd]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.shaman);
    auto uc = f.make_uc();

    std::string long_motd(257, 'x');
    auto r = uc.execute(clan_id, f.chieftain, long_motd);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SetClanMotdError::MotdTooLong);
}
