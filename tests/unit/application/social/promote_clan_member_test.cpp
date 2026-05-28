// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::PromoteClanMember`.

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "application/social/promote_clan_member.hpp"
#include "application/ports/clan_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::PromoteClanMember;
using application::social::PromoteClanMemberError;

// Helper: create a clan with chieftain + peon, save it.
domain::ClanId seed_clan(infra::inmemory::InMemoryClanRepository& repo,
                         domain::AccountId chieftain,
                         domain::AccountId peon) {
    domain::ClanId id{400};
    auto clan_r = domain::social::Clan::create(id, "PRO", "Promote Clan",
                                               chieftain, domain::ClientTag{});
    REQUIRE(clan_r);
    clan_r.value().join(peon, domain::social::ClanRank::Peon);
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
    domain::AccountId outsider{99};

    PromoteClanMember make_uc() { return PromoteClanMember{clans, bus}; }
};

}  // namespace

TEST_CASE("PromoteClanMember: chieftain can promote peon to grunt",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.peon, "grunt");

    REQUIRE(r);
}

TEST_CASE("PromoteClanMember: unknown clan returns ClanNotFound",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::ClanId{999}, f.chieftain, f.peon, "grunt");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == PromoteClanMemberError::ClanNotFound);
}

TEST_CASE("PromoteClanMember: non-chieftain promoter returns InsufficientRank",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);

    // Add a shaman who is NOT chieftain
    domain::AccountId shaman{3};
    auto clan_r = f.clans->find_by_id(clan_id);
    REQUIRE(clan_r);
    clan_r.value()->join(shaman, domain::social::ClanRank::Shaman);
    REQUIRE(f.clans->save(*clan_r.value()));

    auto uc = f.make_uc();
    auto r = uc.execute(clan_id, shaman, f.peon, "grunt");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == PromoteClanMemberError::InsufficientRank);
}

TEST_CASE("PromoteClanMember: target not in clan returns TargetNotMember",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.outsider, "grunt");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == PromoteClanMemberError::TargetNotMember);
}

TEST_CASE("PromoteClanMember: invalid rank string returns InvalidRank",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.peon, "overlord");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == PromoteClanMemberError::InvalidRank);
}
