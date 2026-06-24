// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/social/clan.hpp"
#include "domain/social/clan_rank_wire.hpp"
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

TEST_CASE("ClanRank maps to the correct legacy wire byte",
          "[domain][social][clan][wire]") {
    using domain::social::clan_rank_from_wire;
    using domain::social::clan_rank_to_wire;

    // Legacy bytes (src/bnetd/clan.h): CHIEFTAIN=0x04 .. PEON=0x01. The domain
    // enum is numbered the *other* way (Chieftain=1 .. Peon=4), so the mapping
    // — not the raw enum value — is what must reach the wire / the DB.
    CHECK(clan_rank_to_wire(ClanRank::Chieftain) == 0x04);
    CHECK(clan_rank_to_wire(ClanRank::Shaman)    == 0x03);
    CHECK(clan_rank_to_wire(ClanRank::Grunt)     == 0x02);
    CHECK(clan_rank_to_wire(ClanRank::Peon)      == 0x01);

    // The mapping is NOT the identity (this is the bug being guarded against):
    // a raw cast would send Chieftain as 0x01 (= wire Peon).
    CHECK(clan_rank_to_wire(ClanRank::Chieftain) !=
          static_cast<std::uint8_t>(ClanRank::Chieftain));

    // Round-trips every modeled rank.
    CHECK(clan_rank_from_wire(0x04) == ClanRank::Chieftain);
    CHECK(clan_rank_from_wire(0x03) == ClanRank::Shaman);
    CHECK(clan_rank_from_wire(0x02) == ClanRank::Grunt);
    CHECK(clan_rank_from_wire(0x01) == ClanRank::Peon);
    // The legacy NEW (0x00) probation rank is not modeled -> lowest rank.
    CHECK(clan_rank_from_wire(0x00) == ClanRank::Peon);
}

TEST_CASE("Higher clan rank still outranks lower after the wire mapping",
          "[domain][social][clan][wire]") {
    using domain::social::clan_rank_to_wire;

    // Authority ordering inside the aggregate: a *lower* enum value means a
    // *higher* rank (Chieftain=1 is the strongest). The wire byte ordering is
    // reversed (Chieftain=0x04 is the largest). Both must agree on "Chieftain
    // outranks Peon".
    CHECK(ClanRank::Chieftain < ClanRank::Shaman);  // enum: lower == stronger
    CHECK(ClanRank::Shaman    < ClanRank::Grunt);
    CHECK(ClanRank::Grunt     < ClanRank::Peon);

    // On the wire: larger byte == stronger, and the strength order is preserved.
    CHECK(clan_rank_to_wire(ClanRank::Chieftain) >
          clan_rank_to_wire(ClanRank::Shaman));
    CHECK(clan_rank_to_wire(ClanRank::Shaman) >
          clan_rank_to_wire(ClanRank::Grunt));
    CHECK(clan_rank_to_wire(ClanRank::Grunt) >
          clan_rank_to_wire(ClanRank::Peon));
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

TEST_CASE("Clan::promote_member enforces the Chieftain-only invariant",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();
    // Founder (AccountId{1}) is Chieftain; add a peon member.
    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    (void)c.join(AccountId{2});
    (void)c.drain_events();

    using PO = Clan::PromoteOutcome;

    // A Chieftain may promote a member.
    REQUIRE(c.promote_member(AccountId{1}, AccountId{2}, ClanRank::Grunt) ==
            PO::Promoted);
    REQUIRE(c.members().back().rank == ClanRank::Grunt);

    // A non-Chieftain (the freshly-promoted Grunt) may NOT promote — the
    // authority invariant lives in the aggregate, not the use-case.
    REQUIRE(c.promote_member(AccountId{2}, AccountId{2}, ClanRank::Shaman) ==
            PO::NotAuthorized);
    REQUIRE(c.members().back().rank == ClanRank::Grunt);  // unchanged

    // An unknown target is reported as not-a-member.
    REQUIRE(c.promote_member(AccountId{1}, AccountId{999}, ClanRank::Grunt) ==
            PO::TargetNotMember);

    // A non-member promoter is also unauthorized.
    REQUIRE(c.promote_member(AccountId{42}, AccountId{2}, ClanRank::Peon) ==
            PO::NotAuthorized);
}

TEST_CASE("Clan::kick_member enforces rank + chieftain-protection invariants",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();
    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    (void)c.join(AccountId{2});  // peon
    (void)c.join(AccountId{3});  // peon
    (void)c.drain_events();

    using KO = Clan::KickOutcome;

    // A peon (rank below Shaman) cannot kick.
    REQUIRE(c.kick_member(AccountId{2}, AccountId{3}) == KO::InsufficientRank);
    REQUIRE(c.contains(AccountId{3}));

    // A non-member cannot kick.
    REQUIRE(c.kick_member(AccountId{99}, AccountId{3}) == KO::KickerNotMember);

    // The Chieftain (rank 1) cannot be kicked.
    REQUIRE(c.kick_member(AccountId{1}, AccountId{1}) == KO::CannotKickChieftain);
    REQUIRE(c.contains(AccountId{1}));

    // Unknown target.
    REQUIRE(c.kick_member(AccountId{1}, AccountId{999}) == KO::TargetNotMember);

    // The Chieftain kicks a peon — succeeds and removes the member.
    REQUIRE(c.kick_member(AccountId{1}, AccountId{3}) == KO::Kicked);
    REQUIRE_FALSE(c.contains(AccountId{3}));
}

TEST_CASE("Clan::set_motd enforces authority + length and stores the MOTD",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();
    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    (void)c.join(AccountId{2});  // peon
    (void)c.drain_events();

    using MO = Clan::MotdOutcome;

    // A peon cannot set the MOTD; a non-member cannot either.
    REQUIRE(c.set_motd(AccountId{2}, "hi") == MO::InsufficientRank);
    REQUIRE(c.set_motd(AccountId{99}, "hi") == MO::SetterNotMember);
    REQUIRE(c.motd().empty());

    // Over-length MOTD is rejected.
    REQUIRE(c.set_motd(AccountId{1}, std::string(Clan::kMaxMotdLen + 1, 'x')) ==
            MO::TooLong);
    REQUIRE(c.motd().empty());

    // The Chieftain sets a valid MOTD — stored and readable.
    REQUIRE(c.set_motd(AccountId{1}, "Welcome to PvPGN") == MO::Set);
    REQUIRE(c.motd() == "Welcome to PvPGN");
}

TEST_CASE("Clan::invite_member enforces inviter authority + capacity",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();
    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    (void)c.join(AccountId{2});  // peon
    (void)c.drain_events();

    using IO = Clan::InviteOutcome;

    // A peon cannot invite; a non-member cannot invite.
    REQUIRE(c.invite_member(AccountId{2}, AccountId{5}) == IO::InsufficientRank);
    REQUIRE(c.invite_member(AccountId{99}, AccountId{5}) == IO::InviterNotMember);
    REQUIRE_FALSE(c.contains(AccountId{5}));

    // The Chieftain invites a new peon.
    REQUIRE(c.invite_member(AccountId{1}, AccountId{5}) == IO::Invited);
    REQUIRE(c.contains(AccountId{5}));

    // Re-inviting an existing member is reported.
    REQUIRE(c.invite_member(AccountId{1}, AccountId{5}) == IO::AlreadyMember);
}

TEST_CASE("Clan::is_chieftain reports the founder's authority",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();
    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    (void)c.join(AccountId{2});

    CHECK(c.is_chieftain(AccountId{1}));        // founder
    CHECK_FALSE(c.is_chieftain(AccountId{2}));  // peon
    CHECK_FALSE(c.is_chieftain(AccountId{99})); // non-member
}

TEST_CASE("Clan::leave: members leave, the Chieftain must disband",
          "[domain][social][clan]") {
    auto kStar = ClientTag::parse("WAR3").value();
    auto c = Clan::create(ClanId{1}, "PvP", "PvPGN", AccountId{1}, kStar).value();
    (void)c.join(AccountId{2});  // peon
    (void)c.drain_events();

    using LO = Clan::LeaveOutcome;

    REQUIRE(c.leave(AccountId{99}) == LO::NotMember);
    REQUIRE(c.leave(AccountId{1})  == LO::ChieftainMustDisband);  // founder
    REQUIRE(c.contains(AccountId{1}));

    REQUIRE(c.leave(AccountId{2})  == LO::Left);
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
