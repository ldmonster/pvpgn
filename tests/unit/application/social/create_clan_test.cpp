// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::CreateClan`.

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "application/social/create_clan.hpp"
#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::CreateClan;
using application::social::CreateClanError;
using application::social::CreateClanRequest;

struct Fixture {
    infra::inmemory::InMemoryClanRepository clans;
    infra::inmemory::InMemoryEventBus       bus;

    domain::AccountId founder{42};

    CreateClan make_uc() {
        return CreateClan{
            std::make_shared<infra::inmemory::InMemoryClanRepository>(),
            std::make_shared<infra::inmemory::InMemoryEventBus>()};
    }
};

}  // namespace

TEST_CASE("CreateClan: happy path creates a clan and returns its ID",
          "[application][social][create_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateClanRequest req{f.founder, "TST", "Test Clan"};
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().value() != 0);
}

TEST_CASE("CreateClan: tag too short returns InvalidTag",
          "[application][social][create_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateClanRequest req{f.founder, "T", "Test Clan"};
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreateClanError::InvalidTag);
}

TEST_CASE("CreateClan: empty name returns InvalidName",
          "[application][social][create_clan]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateClanRequest req{f.founder, "TST", ""};
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreateClanError::InvalidName);
}

TEST_CASE("CreateClan: duplicate tag returns TagAlreadyExists",
          "[application][social][create_clan]") {
    auto clans = std::make_shared<infra::inmemory::InMemoryClanRepository>();
    auto bus   = std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId founder{42};

    // Create first clan
    CreateClan uc{clans, bus};
    CreateClanRequest req1{founder, "TST", "Test Clan One"};
    REQUIRE(uc.execute(req1));

    // Attempt to create second clan with same tag
    CreateClanRequest req2{founder, "TST", "Test Clan Two"};
    auto r = uc.execute(req2);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreateClanError::TagAlreadyExists);
}
