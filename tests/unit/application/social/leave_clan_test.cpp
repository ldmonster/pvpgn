// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::LeaveClan`.

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "application/social/leave_clan.hpp"
#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::LeaveClan;
using application::social::LeaveClanError;

// Helper: create a clan with chieftain + grunt, save it.
domain::ClanId seed_clan(infra::inmemory::InMemoryClanRepository& repo,
                         domain::AccountId chieftain,
                         domain::AccountId grunt) {
    domain::ClanId id{700};
    auto clan_r = domain::social::Clan::create(id, "LVE", "Leave Clan",
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

    LeaveClan make_uc() { return LeaveClan{clans, bus}; }
};

}  // namespace

TEST_CASE("LeaveClan: grunt can leave the clan",
          "[application][social][leave_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.grunt);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.grunt);

    REQUIRE(r);
}

TEST_CASE("LeaveClan: unknown clan returns ClanNotFound",
          "[application][social][leave_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::ClanId{999}, f.grunt);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveClanError::ClanNotFound);
}

TEST_CASE("LeaveClan: account not in clan returns NotAMember",
          "[application][social][leave_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.grunt);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.outsider);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveClanError::NotAMember);
}

TEST_CASE("LeaveClan: chieftain leaving returns LeaderMustDisband",
          "[application][social][leave_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.grunt);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveClanError::LeaderMustDisband);
}
