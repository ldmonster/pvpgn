// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_ingame_test.cpp
/// Unit tests for the InGame-state handlers, pairing
/// `connection_fsm_ingame.cpp` (on_leave_game / on_d2_char_select /
/// on_warcraft_general). Covers the SID_STOPADV → InChannel transition and
/// game lifecycle/event bookkeeping, the D2 character binding consumed by the
/// char-select handler, and the WAR3 route token set by SID_WARCRAFTGENERAL.

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;

// --- SID_STOPADV: InGame → InChannel ---------------------------------------

TEST_CASE("ConnectionFsm: SID_STOPADV transitions InGame→InChannel",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    reach_in_game_via_start(fsm);

    const std::uint32_t gid = fsm.game_id();
    ctx.sent.clear();
    ctx.game_events.clear();

    auto result = fsm.dispatch(sid::kCloseGame,
                               std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(fsm.game_id() == 0u);
    CHECK_FALSE(ctx.closed);

    // on_game_left must have been called with the correct game_id
    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind    == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[0].game_id == gid);
}

TEST_CASE("ConnectionFsm: SID_STOPADV in InChannel is silently ignored",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    // STOPADV while not in a game — should be silently ignored
    auto result = fsm.dispatch(sid::kCloseGame,
                               std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.game_events.empty());
}

// --- Full game lifecycles --------------------------------------------------

TEST_CASE("ConnectionFsm: full StartGame→LeaveGame lifecycle",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm, "alice");
    reach_in_channel(fsm, "alice");
    ctx.game_events.clear();

    // Start a game
    auto sg = make_start_game("AliceGame");
    REQUIRE(fsm.dispatch(sid::kStartGame1,
                         std::span<const std::byte>{sg}).has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    const std::uint32_t gid = fsm.game_id();
    CHECK(gid != 0u);

    // Leave the game
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(fsm.game_id() == 0u);

    // Verify event sequence: Created then Left
    REQUIRE(ctx.game_events.size() == 2);
    CHECK(ctx.game_events[0].kind    == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[0].game_id == gid);
    CHECK(ctx.game_events[1].kind    == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[1].game_id == gid);

    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: full JoinGame→LeaveGame lifecycle",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm, "bob");
    reach_in_channel(fsm, "bob");
    ctx.game_events.clear();

    // Join a game
    auto jg = make_join_game_pkt("BobsGame");
    REQUIRE(fsm.dispatch(sid::kJoinGame,
                         std::span<const std::byte>{jg}).has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    const std::uint32_t gid = fsm.game_id();
    CHECK(gid != 0u);

    // Leave the game
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(fsm.game_id() == 0u);

    // Verify event sequence: Joined then Left
    REQUIRE(ctx.game_events.size() == 2);
    CHECK(ctx.game_events[0].kind    == FakeContext::GameEvent::Kind::Joined);
    CHECK(ctx.game_events[0].game_id == gid);
    CHECK(ctx.game_events[1].kind    == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[1].game_id == gid);

    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: game_id observer reflects InGame state",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    CHECK(fsm.game_id() == 0u);  // not in a game yet

    reach_in_game_via_start(fsm);
    CHECK(fsm.game_id() != 0u);  // now in a game

    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());
    CHECK(fsm.game_id() == 0u);  // left the game
}

TEST_CASE("ConnectionFsm: consecutive games receive distinct game IDs",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.game_events.clear();

    // First game
    reach_in_game_via_start(fsm, "Game1");
    const std::uint32_t gid1 = fsm.game_id();
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());

    // Second game
    reach_in_game_via_start(fsm, "Game2");
    const std::uint32_t gid2 = fsm.game_id();
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());

    CHECK(gid1 != 0u);
    CHECK(gid2 != 0u);
    CHECK(gid1 != gid2);

    // Four events: Created, Left, Created, Left
    REQUIRE(ctx.game_events.size() == 4);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[1].kind == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[2].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[3].kind == FakeContext::GameEvent::Kind::Left);
}

// --- D2 character binding (consumed by on_d2_char_select) ------------------

TEST_CASE("ConnectionFsm: bind_d2_character sets has_d2_character true",
          "[connection_fsm][ingame][d2char]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    REQUIRE_FALSE(fsm.has_d2_character());

    fsm.bind_d2_character("Sorceress", 1u, 42u);

    REQUIRE(fsm.has_d2_character());
}

TEST_CASE("ConnectionFsm: bind_d2_character stores name correctly",
          "[connection_fsm][ingame][d2char]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.bind_d2_character("Necromancer", 2u, 30u);

    REQUIRE(fsm.d2_char_name().has_value());
    CHECK(*fsm.d2_char_name() == "Necromancer");
}

TEST_CASE("ConnectionFsm: bind_d2_character stores class and level",
          "[connection_fsm][ingame][d2char]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.bind_d2_character("Paladin", 3u, 99u);

    REQUIRE(fsm.d2_char_class().has_value());
    CHECK(*fsm.d2_char_class() == 3u);
    REQUIRE(fsm.d2_char_level().has_value());
    CHECK(*fsm.d2_char_level() == 99u);
}

TEST_CASE("ConnectionFsm: bind_d2_character overwrites previous binding",
          "[connection_fsm][ingame][d2char]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.bind_d2_character("Amazon", 0u, 10u);
    fsm.bind_d2_character("Barbarian", 4u, 55u);

    REQUIRE(fsm.has_d2_character());
    CHECK(*fsm.d2_char_name()  == "Barbarian");
    CHECK(*fsm.d2_char_class() == 4u);
    CHECK(*fsm.d2_char_level() == 55u);
}

TEST_CASE("ConnectionFsm: d2_char accessors return empty optional before binding",
          "[connection_fsm][ingame][d2char]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    CHECK_FALSE(fsm.d2_char_name().has_value());
    CHECK_FALSE(fsm.d2_char_class().has_value());
    CHECK_FALSE(fsm.d2_char_level().has_value());
}

// --- WAR3 route token (set by on_warcraft_general) -------------------------

TEST_CASE("ConnectionFsm: war3_route_token is empty before set",
          "[connection_fsm][ingame][war3token]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    CHECK_FALSE(fsm.war3_route_token().has_value());
}

TEST_CASE("ConnectionFsm: set_war3_route_token stores the token",
          "[connection_fsm][ingame][war3token]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.set_war3_route_token(0xDEADBEEFu);

    REQUIRE(fsm.war3_route_token().has_value());
    CHECK(*fsm.war3_route_token() == 0xDEADBEEFu);
}

TEST_CASE("ConnectionFsm: set_war3_route_token overwrites previous value",
          "[connection_fsm][ingame][war3token]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.set_war3_route_token(0x11111111u);
    fsm.set_war3_route_token(0x22222222u);

    REQUIRE(fsm.war3_route_token().has_value());
    CHECK(*fsm.war3_route_token() == 0x22222222u);
}

TEST_CASE("ConnectionFsm: set_war3_route_token accepts zero token",
          "[connection_fsm][ingame][war3token]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.set_war3_route_token(0u);

    REQUIRE(fsm.war3_route_token().has_value());
    CHECK(*fsm.war3_route_token() == 0u);
}
