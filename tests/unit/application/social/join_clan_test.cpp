// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::JoinClan`.

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "application/social/join_clan.hpp"
#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::JoinClan;
using application::social::JoinClanError;

// Helper: create a clan with a chieftain and save it.
domain::ClanId seed_clan(infra::inmemory::InMemoryClanRepository& repo,
                         domain::AccountId chieftain) {
    domain::ClanId id{600};
    auto clan_r = domain::social::Clan::create(id, "JCL", "Join Clan",
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
    domain::AccountId newcomer{2};

    JoinClan make_uc() { return JoinClan{clans, bus}; }
};

}  // namespace

TEST_CASE("JoinClan: new member can join an existing clan",
          "[application][social][join_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.newcomer);

    REQUIRE(r);
}

TEST_CASE("JoinClan: unknown clan returns ClanNotFound",
          "[application][social][join_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::ClanId{999}, f.newcomer);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinClanError::ClanNotFound);
}

TEST_CASE("JoinClan: joining twice returns AlreadyInClan",
          "[application][social][join_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);
    auto uc = f.make_uc();

    // First join succeeds
    REQUIRE(uc.execute(clan_id, f.newcomer));

    // Second join fails
    auto r = uc.execute(clan_id, f.newcomer);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinClanError::AlreadyInClan);
}

TEST_CASE("JoinClan: chieftain joining their own clan returns AlreadyInClan",
          "[application][social][join_clan]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain);
    auto uc = f.make_uc();

    // Chieftain is already a member (added during create)
    auto r = uc.execute(clan_id, f.chieftain);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinClanError::AlreadyInClan);
}
