// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_inchannel_test.cpp
/// Unit tests for the InChannel-state handlers, pairing
/// `connection_fsm_inchannel.cpp` (on_join_channel / on_chat_command /
/// on_leave_channel / on_start_game / on_join_game). Covers channel join/chat
/// acceptance, wrong-state rejection, the LEAVECHAT → LoggedIn transition, the
/// StartGame/JoinGame → InGame transitions (with GameInfo passthrough), and
/// their rejection outside InChannel.

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;

// --- SID_JOINCHANNEL -------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_JOINCHANNEL in InChannel is accepted",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_JOINCHANNEL in LoggedIn state is rejected",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// --- SID_CHATCOMMAND -------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_CHATCOMMAND in InChannel is accepted",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_chat_command("hello world");
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_CHATCOMMAND in Connecting state is rejected",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_chat_command("hello");
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// --- SID_LEAVECHAT: InChannel → LoggedIn -----------------------------------

TEST_CASE("ConnectionFsm: SID_LEAVECHAT transitions InChannel→LoggedIn",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kLeaveChannel,
                               std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK_FALSE(ctx.closed);
}

// --- StartGame / JoinGame: InChannel → InGame ------------------------------

TEST_CASE("ConnectionFsm: SID_STARTADVEX transitions InChannel→InGame",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_start_game("MyGame", "", "PXES");
    auto result  = fsm.dispatch(sid::kStartGame1,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK(fsm.game_id() != 0u);
    CHECK_FALSE(ctx.closed);

    // FSM must send a SID_STARTADVEX reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kStartGame1);

    // on_game_created must have been called exactly once
    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[0].game_id == fsm.game_id());
    CHECK(ctx.game_events[0].info.game_name == "MyGame");
}

TEST_CASE("ConnectionFsm: SID_STARTADVEX3 transitions InChannel→InGame",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_start_game("AdvGame3", "", "WAR3");
    auto result  = fsm.dispatch(sid::kStartGame3,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK(fsm.game_id() != 0u);

    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[0].info.game_name == "AdvGame3");
}

TEST_CASE("ConnectionFsm: SID_GETADVLISTEX transitions InChannel→InGame",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_join_game_pkt("FriendGame", "secret");
    auto result  = fsm.dispatch(sid::kJoinGame,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK(fsm.game_id() != 0u);
    CHECK_FALSE(ctx.closed);

    // FSM must send a SID_GETADVLISTEX reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kJoinGame);

    // on_game_joined must have been called exactly once
    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Joined);
    CHECK(ctx.game_events[0].game_id == fsm.game_id());
    CHECK(ctx.game_events[0].info.game_name == "FriendGame");
    CHECK(ctx.game_events[0].info.password  == "secret");
}

// --- StartGame / JoinGame rejected outside InChannel -----------------------

TEST_CASE("ConnectionFsm: SID_STARTADVEX in LoggedIn state is rejected",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_start_game("BadGame");
    auto result  = fsm.dispatch(sid::kStartGame1,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    // No game event should have been fired
    CHECK(ctx.game_events.empty());
}

TEST_CASE("ConnectionFsm: SID_GETADVLISTEX in LoggedIn state is rejected",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_join_game_pkt("BadGame");
    auto result  = fsm.dispatch(sid::kJoinGame,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    CHECK(ctx.game_events.empty());
}

TEST_CASE("ConnectionFsm: SID_STARTADVEX in Connecting state is rejected",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_start_game("EarlyGame");
    auto result  = fsm.dispatch(sid::kStartGame1,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    CHECK(ctx.game_events.empty());
}

TEST_CASE("ConnectionFsm: SID_GETADVLISTEX in Connecting state is rejected",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_join_game_pkt("EarlyJoin");
    auto result  = fsm.dispatch(sid::kJoinGame,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    CHECK(ctx.game_events.empty());
}

// --- GameInfo metadata passthrough -----------------------------------------

TEST_CASE("ConnectionFsm: StartGame passes GameInfo metadata to context",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.game_events.clear();

    // game_type = 3 (Cooperative), with password
    auto payload = make_start_game("CoopMission", "pw123", "STATS", 3u);
    REQUIRE(fsm.dispatch(sid::kStartGame1,
                         std::span<const std::byte>{payload}).has_value());

    REQUIRE(ctx.game_events.size() == 1);
    const auto& ev = ctx.game_events[0];
    CHECK(ev.kind              == FakeContext::GameEvent::Kind::Created);
    CHECK(ev.info.game_name    == "CoopMission");
    CHECK(ev.info.password     == "pw123");
    CHECK(ev.info.game_stats   == "STATS");
    CHECK(ev.info.game_type    == GameType::Cooperative);
}

TEST_CASE("ConnectionFsm: JoinGame passes GameInfo metadata to context",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.game_events.clear();

    // game_type = 2 (OneOnOne), with password
    auto payload = make_join_game_pkt("DuelArena", "duel", "", 2u);
    REQUIRE(fsm.dispatch(sid::kJoinGame,
                         std::span<const std::byte>{payload}).has_value());

    REQUIRE(ctx.game_events.size() == 1);
    const auto& ev = ctx.game_events[0];
    CHECK(ev.kind           == FakeContext::GameEvent::Kind::Joined);
    CHECK(ev.info.game_name == "DuelArena");
    CHECK(ev.info.password  == "duel");
    CHECK(ev.info.game_type == GameType::OneOnOne);
}
