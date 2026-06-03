// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_inchannel_branches_test.cpp
/// Branch coverage for connection_fsm_inchannel.cpp that the main inchannel
/// test does not reach: the game_type switch arms in on_start_game /
/// on_join_game (the existing tests only use the default Melee value), the
/// empty-channel-name early return in on_join_channel, and the out-of-order
/// rejection of StartGame/JoinGame outside InChannel.

#include "connection_fsm_test_fixtures.hpp"

using namespace pvpgn::test::connection_fsm;
using GT = pvpgn::application::connection::GameType;

namespace {

// ConnectionFsm is non-movable, so drive an already-constructed fsm to InChannel
// in place and clear the recorded traffic.
inline void setup_in_channel(ConnectionFsm& fsm, FakeContext& ctx) {
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();
}

}  // namespace

// --- on_start_game: every game_type switch arm -----------------------------

TEST_CASE("ConnectionFsm: STARTADVEX maps each game_type byte to GameType",
          "[connection_fsm][inchannel][ingame]") {
    struct Case { std::uint32_t raw; GT expected; };
    const Case cases[] = {
        {1, GT::FreeForAll}, {2, GT::OneOnOne}, {3, GT::Cooperative},
        {4, GT::Custom},     {0, GT::Melee},    {99, GT::Melee},
    };

    for (const auto& c : cases) {
        FakeContext ctx;
        ConnectionFsm fsm{ctx};
        setup_in_channel(fsm, ctx);

        auto payload = make_start_game("G", "", "PXES", c.raw);
        auto result  = fsm.dispatch(sid::kStartGame1,
                                    std::span<const std::byte>{payload});

        REQUIRE(result.has_value());
        CHECK(fsm.state() == ConnectionState::InGame);
        REQUIRE(ctx.game_events.size() == 1);
        CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Created);
        CHECK(ctx.game_events[0].info.game_type == c.expected);
    }
}

// --- on_join_game: every game_type switch arm ------------------------------

TEST_CASE("ConnectionFsm: GETADVLISTEX maps each game_type byte to GameType",
          "[connection_fsm][inchannel][ingame]") {
    struct Case { std::uint32_t raw; GT expected; };
    const Case cases[] = {
        {1, GT::FreeForAll}, {2, GT::OneOnOne}, {3, GT::Cooperative},
        {4, GT::Custom},     {0, GT::Melee},    {7, GT::Melee},
    };

    for (const auto& c : cases) {
        FakeContext ctx;
        ConnectionFsm fsm{ctx};
        setup_in_channel(fsm, ctx);

        auto payload = make_join_game_pkt("G", "", "PXES", c.raw);
        auto result  = fsm.dispatch(sid::kJoinGame,
                                    std::span<const std::byte>{payload});

        REQUIRE(result.has_value());
        CHECK(fsm.state() == ConnectionState::InGame);
        REQUIRE(ctx.game_events.size() == 1);
        CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Joined);
        CHECK(ctx.game_events[0].info.game_type == c.expected);
    }
}

// --- on_join_channel: empty channel name is ignored ------------------------

TEST_CASE("ConnectionFsm: JOINCHANNEL with empty name is a no-op ok",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    setup_in_channel(fsm, ctx);

    auto payload = make_join_channel("");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());          // accepted, but ignored
    CHECK(fsm.state() == ConnectionState::InChannel);  // no transition
    CHECK(ctx.sent.empty());              // no EID_CHANNEL emitted
    CHECK_FALSE(ctx.closed);
}

// --- StartGame / JoinGame rejected outside InChannel -----------------------

TEST_CASE("ConnectionFsm: STARTADVEX outside InChannel is rejected",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);  // LoggedIn, not InChannel
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_start_game("G", "", "PXES", 1);
    auto result  = fsm.dispatch(sid::kStartGame1,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    // The out-of-order guard rejects without creating a game (reject() also
    // tears the connection down, so the exact post-state is not LoggedIn).
    CHECK(fsm.state() != ConnectionState::InGame);
    CHECK(ctx.game_events.empty());
}

TEST_CASE("ConnectionFsm: GETADVLISTEX outside InChannel is rejected",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);  // LoggedIn, not InChannel
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_join_game_pkt("G", "", "PXES", 2);
    auto result  = fsm.dispatch(sid::kJoinGame,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    // The out-of-order guard rejects without creating a game (reject() also
    // tears the connection down, so the exact post-state is not LoggedIn).
    CHECK(fsm.state() != ConnectionState::InGame);
    CHECK(ctx.game_events.empty());
}
