// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional outcome tests for `application::social::PromoteClanMember`.
// Mirrors promote_clan_member_test.cpp for fakes/helpers but exercises the
// rank-string parsing arms ("peon", "shaman", "chieftain") and the
// promote-to-current-rank case that the base test does not reach.

#include <memory>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/social/promote_clan_member.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
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
    domain::ClanId id{410};
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

    PromoteClanMember make_uc() { return PromoteClanMember{clans, bus}; }
};

// Returns the rank the target ended up at after a successful promote, so we can
// assert the wire-rank string was parsed into the right `ClanRank`.
domain::social::ClanRank rank_of(infra::inmemory::InMemoryClanRepository& repo,
                                 domain::ClanId clan_id,
                                 domain::AccountId who) {
    auto clan_r = repo.find_by_id(clan_id);
    REQUIRE(clan_r);
    for (const auto& m : clan_r.value()->members()) {
        if (m.account == who) return m.rank;
    }
    FAIL("target not a member");
    return domain::social::ClanRank::Peon;  // unreachable
}

}  // namespace

TEST_CASE("PromoteClanMember: chieftain promotes peon to shaman",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.peon, "shaman");

    REQUIRE(r);
    REQUIRE(rank_of(*f.clans, clan_id, f.peon) ==
            domain::social::ClanRank::Shaman);
}

TEST_CASE("PromoteClanMember: chieftain promotes target to chieftain rank",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);
    auto uc = f.make_uc();

    auto r = uc.execute(clan_id, f.chieftain, f.peon, "chieftain");

    REQUIRE(r);
    REQUIRE(rank_of(*f.clans, clan_id, f.peon) ==
            domain::social::ClanRank::Chieftain);
}

TEST_CASE("PromoteClanMember: chieftain demotes a grunt back to peon",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);
    auto uc = f.make_uc();

    // First promote to grunt, then demote to peon ("peon" parse arm).
    REQUIRE(uc.execute(clan_id, f.chieftain, f.peon, "grunt"));
    auto r = uc.execute(clan_id, f.chieftain, f.peon, "peon");

    REQUIRE(r);
    REQUIRE(rank_of(*f.clans, clan_id, f.peon) ==
            domain::social::ClanRank::Peon);
}

TEST_CASE("PromoteClanMember: promoting a member to its current rank succeeds",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = seed_clan(*f.clans, f.chieftain, f.peon);
    auto uc = f.make_uc();

    // The peon is already a Peon — set_rank is a no-op but still authorized,
    // so the aggregate reports Promoted and the use-case succeeds.
    auto r = uc.execute(clan_id, f.chieftain, f.peon, "peon");

    REQUIRE(r);
    REQUIRE(rank_of(*f.clans, clan_id, f.peon) ==
            domain::social::ClanRank::Peon);
}
