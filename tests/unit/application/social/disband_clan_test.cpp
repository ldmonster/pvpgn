// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::DisbandClan`.

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "application/social/disband_clan.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::DisbandClan;
using application::social::DisbandClanError;

// Helper: create a clan with a chieftain already saved in the repo.
domain::ClanId seed_clan(infra::inmemory::InMemoryClanRepository& repo,
                         domain::AccountId chieftain) {
    domain::ClanId id{100};
    auto clan_r = domain::social::Clan::create(id, "TST", "Test Clan",
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
    domain::AccountId peon{2};

    DisbandClan make_uc() { return DisbandClan{clans, bus}; }
};

}  // namespace

TEST_CASE("DisbandClan: chieftain can disband their clan",
          "[application][social][disband_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain);

    REQUIRE(r);
}

TEST_CASE("DisbandClan: unknown clan returns ClanNotFound",
          "[application][social][disband_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::ClanId{999}, f.chieftain);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == DisbandClanError::ClanNotFound);
}

TEST_CASE("DisbandClan: non-chieftain member returns NotChieftain",
          "[application][social][disband_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);

    // Add peon to the clan via the repo
    auto clan_r = f.clans->find_by_id(clan_id);
    REQUIRE(clan_r);
    clan_r.value()->join(f.peon, domain::social::ClanRank::Peon);
    REQUIRE(f.clans->save(*clan_r.value()));

    auto uc = f.make_uc();
    auto r = uc.execute(clan_id, f.peon);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == DisbandClanError::NotChieftain);
}

TEST_CASE("DisbandClan: account not in clan returns NotChieftain",
          "[application][social][disband_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);
    auto uc = f.make_uc();

    domain::AccountId outsider{99};
    auto r = uc.execute(clan_id, outsider);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == DisbandClanError::NotChieftain);
}
