// SPDX-License-Identifier: GPL-2.0-or-later
//
// Verification test for `application::ports::IClanRepository` interface.
// This test verifies the port interface contract and the domain::social::Clan
// aggregate that works with it.

#include <catch2/catch_test_macros.hpp>

#include "domain/social/ports.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"

namespace {

using namespace pvpgn;

struct Fixture {
    domain::ClanId clan_id{1};
    domain::AccountId founder_id{42};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    domain::social::Clan make_clan() {
        auto result = domain::social::Clan::create(
            clan_id, "TEST", "Test Clan", founder_id, star_tag);
        REQUIRE(result);
        return result.value();
    }
};

}  // namespace

TEST_CASE("ClanRepository interface: Clan aggregate creation",
          "[application][ports][clan_repository]") {
    Fixture f;
    auto clan = f.make_clan();

    REQUIRE(clan.id() == f.clan_id);
    REQUIRE(clan.tag() == "TEST");
    REQUIRE(clan.name() == "Test Clan");
    REQUIRE(clan.client() == f.star_tag);
    REQUIRE(clan.size() == 1);  // Founder is auto-added
}

TEST_CASE("ClanRepository interface: Clan tag validation",
          "[application][ports][clan_repository]") {
    Fixture f;
    
    // Tag too short
    auto short_tag = domain::social::Clan::create(
        domain::ClanId{2}, "T", "Test", f.founder_id, f.star_tag);
    REQUIRE_FALSE(short_tag);

    // Tag too long
    auto long_tag = domain::social::Clan::create(
        domain::ClanId{3}, "VERYLONGTAG", "Test", f.founder_id, f.star_tag);
    REQUIRE_FALSE(long_tag);

    // Valid 3-char tag
    auto valid_tag = domain::social::Clan::create(
        domain::ClanId{4}, "ABC", "Test", f.founder_id, f.star_tag);
    REQUIRE(valid_tag);
}

TEST_CASE("ClanRepository interface: Clan name validation",
          "[application][ports][clan_repository]") {
    Fixture f;

    // Name too long (> 25 chars)
    auto long_name = domain::social::Clan::create(
        domain::ClanId{2}, "TST", "This is a very long clan name that exceeds", f.founder_id, f.star_tag);
    REQUIRE_FALSE(long_name);

    // Valid name
    auto valid_name = domain::social::Clan::create(
        domain::ClanId{3}, "TST", "Valid Clan Name", f.founder_id, f.star_tag);
    REQUIRE(valid_name);
}

TEST_CASE("ClanRepository interface: Clan members",
          "[application][ports][clan_repository]") {
    Fixture f;
    auto clan = f.make_clan();

    // Founder should be in members
    const auto& members = clan.members();
    REQUIRE(members.size() >= 1);
    
    // Founder should be Chieftain
    bool found_founder = false;
    for (const auto& member : members) {
        if (member.account == f.founder_id) {
            found_founder = true;
            REQUIRE(member.rank == domain::social::ClanRank::Chieftain);
        }
    }
    REQUIRE(found_founder);
}

TEST_CASE("ClanRepository interface: Clan rehydration",
          "[application][ports][clan_repository]") {
    Fixture f;
    auto clan = f.make_clan();
    auto members = clan.members();

    // Rehydrate from persistence snapshot
    auto rehydrated = domain::social::Clan::rehydrate(
        clan.id(), clan.tag(), clan.name(), clan.client(), members);

    REQUIRE(rehydrated.id() == clan.id());
    REQUIRE(rehydrated.tag() == clan.tag());
    REQUIRE(rehydrated.name() == clan.name());
    REQUIRE(rehydrated.members().size() == members.size());
}
