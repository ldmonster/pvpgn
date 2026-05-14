// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/social/clan.hpp"
#include "domain/social/friend_list.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::ClanId;
using domain::ClientTag;
using domain::social::Clan;
using domain::social::ClanRank;
using domain::social::FriendList;

TEST_CASE("FriendList: add -> AddOutcome::Added + FriendAdded event",
          "[domain][social][friend]") {
    FriendList fl{AccountId{1}};
    REQUIRE(fl.add(AccountId{2}) == FriendList::AddOutcome::Added);
    REQUIRE(fl.size() == 1);
    auto evs = fl.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::FriendAdded>(evs[0]));
}

TEST_CASE("FriendList: add self / duplicate / capacity",
          "[domain][social][friend]") {
    FriendList fl{AccountId{1}};
    REQUIRE(fl.add(AccountId{1}) == FriendList::AddOutcome::Self);
    REQUIRE(fl.add(AccountId{2}) == FriendList::AddOutcome::Added);
    REQUIRE(fl.add(AccountId{2}) == FriendList::AddOutcome::AlreadyPresent);

    for (std::uint32_t i = 3; i < 3 + FriendList::kMaxFriends - 1; ++i) {
        REQUIRE(fl.add(AccountId{i}) == FriendList::AddOutcome::Added);
    }
    REQUIRE(fl.size() == FriendList::kMaxFriends);
    REQUIRE(fl.add(AccountId{999}) == FriendList::AddOutcome::Full);
}

TEST_CASE("FriendList: remove emits FriendRemoved, missing target returns false",
          "[domain][social][friend]") {
    FriendList fl{AccountId{1}};
    (void)fl.add(AccountId{2});
    (void)fl.drain_events();
    REQUIRE(fl.remove(AccountId{42}) == false);
    REQUIRE(fl.remove(AccountId{2}) == true);
    auto evs = fl.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::FriendRemoved>(evs[0]));
}

TEST_CASE("Clan::create rejects bad tag/name and emits Created+MemberJoined",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();

    REQUIRE_FALSE(Clan::create(ClanId{1}, "x", "Name", AccountId{1}, kStar).has_value());
    REQUIRE_FALSE(Clan::create(ClanId{1}, "TOOMANY", "Name", AccountId{1}, kStar).has_value());
    REQUIRE_FALSE(Clan::create(ClanId{1}, "PvP", "", AccountId{1}, kStar).has_value());

    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    REQUIRE(c.tag() == "PvP");
    REQUIRE(c.size() == 1);
    REQUIRE(c.members().front().rank == ClanRank::Chieftain);

    auto evs = c.drain_events();
    REQUIRE(evs.size() == 2);
    REQUIRE(std::holds_alternative<domain::events::ClanCreated>(evs[0]));
    REQUIRE(std::holds_alternative<domain::events::ClanMemberJoined>(evs[1]));
}

TEST_CASE("Clan: join / remove / set_rank semantics",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();
    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    (void)c.drain_events();

    REQUIRE(c.join(AccountId{2}) == Clan::JoinOutcome::Joined);
    REQUIRE(c.join(AccountId{2}) == Clan::JoinOutcome::AlreadyMember);
    REQUIRE(c.contains(AccountId{2}));

    REQUIRE(c.set_rank(AccountId{2}, ClanRank::Shaman));
    REQUIRE(c.members().back().rank == ClanRank::Shaman);
    REQUIRE_FALSE(c.set_rank(AccountId{999}, ClanRank::Grunt));

    REQUIRE(c.remove(AccountId{2}));
    REQUIRE_FALSE(c.contains(AccountId{2}));
}

#include "domain/social/team.hpp"

using domain::TeamId;
using domain::social::Team;

TEST_CASE("Team::create validates size and uniqueness",
          "[domain][social][team]") {
    auto kStar = ClientTag::parse("WAR3").value();
    REQUIRE_FALSE(Team::create(TeamId{1}, {AccountId{1}}, kStar).has_value());
    REQUIRE_FALSE(Team::create(TeamId{1},
        {AccountId{1}, AccountId{2}, AccountId{3}, AccountId{4}, AccountId{5}},
        kStar).has_value());
    REQUIRE_FALSE(Team::create(TeamId{1}, {AccountId{1}, AccountId{1}}, kStar).has_value());

    auto t = Team::create(TeamId{1}, {AccountId{1}, AccountId{2}}, kStar).value();
    REQUIRE(t.size() == 2);
    REQUIRE(t.contains(AccountId{1}));
    auto evs = t.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::TeamCreated>(evs[0]));
}

TEST_CASE("Team::disband is one-way and idempotent",
          "[domain][social][team]") {
    auto kStar = ClientTag::parse("WAR3").value();
    auto t = Team::create(TeamId{1}, {AccountId{1}, AccountId{2}}, kStar).value();
    (void)t.drain_events();

    t.disband();
    REQUIRE(t.disbanded());
    auto evs = t.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::TeamDisbanded>(evs[0]));

    t.disband();  // idempotent
    REQUIRE(t.drain_events().empty());
}
