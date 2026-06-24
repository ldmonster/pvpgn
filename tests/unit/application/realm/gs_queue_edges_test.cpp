// SPDX-License-Identifier: GPL-2.0-or-later
//
// Edge-case unit tests for GameServerQueue (gs_queue.cpp).
// Complements gs_queue_test.cpp by exercising not-found branches, server
// selection filters, stale cleanup, and re-registration semantics.

#include <catch2/catch_test_macros.hpp>
#include "application/realm/gs_queue.hpp"
#include <chrono>

namespace pvpgn::application::realm {

TEST_CASE("GameServerQueue - leave non-existent game fails", "[application][realm]") {
    GameServerQueue queue;
    auto result = queue.leave_game("NoGame", "player1");
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - close non-existent game fails", "[application][realm]") {
    GameServerQueue queue;
    auto result = queue.close_game("NoGame");
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - leave absent player is a no-op success",
          "[application][realm]") {
    GameServerQueue queue;

    GameInfo info;
    info.game_name = "TestGame";
    REQUIRE(queue.create_game(info));

    REQUIRE(queue.join_game("TestGame", "player1"));

    // Removing a player who is not in the game still succeeds and leaves the
    // existing roster untouched.
    auto leave_result = queue.leave_game("TestGame", "ghost");
    REQUIRE(leave_result);

    auto find_result = queue.find_game("TestGame");
    REQUIRE(find_result);
    CHECK(find_result.value().players.size() == 1);
    CHECK(find_result.value().players[0] == "player1");
}

TEST_CASE("GameServerQueue - empty list on fresh queue", "[application][realm]") {
    GameServerQueue queue;
    CHECK(queue.list_servers().empty());
    CHECK(queue.list_games(false).empty());
    CHECK(queue.list_games(true).empty());
}

TEST_CASE("GameServerQueue - register replaces existing server at same address",
          "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo first;
    first.address = "gs.example.com";
    first.port    = 4000;
    queue.register_server(first);

    GameServerInfo second;
    second.address = "gs.example.com";  // same key
    second.port    = 4100;
    queue.register_server(second);

    // Same address -> single entry, latest wins.
    CHECK(queue.list_servers().size() == 1);
    auto found = queue.find_server("gs.example.com");
    REQUIRE(found);
    CHECK(found.value().port == 4100);
}

TEST_CASE("GameServerQueue - update_heartbeat on unknown server is a no-op",
          "[application][realm]") {
    GameServerQueue queue;
    // Should not throw or create an entry.
    queue.update_heartbeat("ghost.example.com");
    CHECK(queue.list_servers().empty());
}

TEST_CASE("GameServerQueue - remove absent server is a no-op", "[application][realm]") {
    GameServerQueue queue;
    queue.remove_server("ghost.example.com");
    CHECK(queue.list_servers().empty());
}

TEST_CASE("GameServerQueue - select_server skips offline servers",
          "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo off;
    off.address = "off";
    off.status  = GameServerStatus::offline;
    queue.register_server(off);

    auto result = queue.select_server(false);
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - select_server skips full servers", "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo full;
    full.address         = "full";
    full.status          = GameServerStatus::online;
    full.current_players = 8;
    full.max_players     = 8;
    queue.register_server(full);

    auto result = queue.select_server(false);
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - select_server skips classic when expansion requested",
          "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo classic;
    classic.address      = "classic";
    classic.status       = GameServerStatus::online;
    classic.is_expansion = false;
    classic.max_players  = 10;
    queue.register_server(classic);

    auto result = queue.select_server(true);  // require expansion
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - select_server picks least-loaded eligible server",
          "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo busy;
    busy.address         = "busy";
    busy.status          = GameServerStatus::online;
    busy.current_players = 50;
    busy.max_players     = 255;
    queue.register_server(busy);

    GameServerInfo idle;
    idle.address         = "idle";
    idle.status          = GameServerStatus::online;
    idle.current_players = 1;
    idle.max_players     = 255;
    queue.register_server(idle);

    auto result = queue.select_server(false);
    REQUIRE(result);
    CHECK(result.value().address == "idle");
}

TEST_CASE("GameServerQueue - cleanup_stale removes servers past the timeout",
          "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo stale;
    stale.address        = "stale";
    stale.last_heartbeat = std::chrono::system_clock::now() - std::chrono::hours(1);
    queue.register_server(stale);

    GameServerInfo fresh;
    fresh.address        = "fresh";
    fresh.last_heartbeat = std::chrono::system_clock::now();
    queue.register_server(fresh);

    queue.cleanup_stale(std::chrono::seconds(60));

    CHECK_FALSE(queue.find_server("stale"));
    CHECK(queue.find_server("fresh"));
}

TEST_CASE("GameServerQueue - cleanup_stale keeps recently active servers",
          "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo info;
    info.address        = "recent";
    info.last_heartbeat = std::chrono::system_clock::now();
    queue.register_server(info);

    queue.cleanup_stale(std::chrono::seconds(60));
    CHECK(queue.find_server("recent"));
}

TEST_CASE("GameServerQueue - update_heartbeat brings server online",
          "[application][realm]") {
    GameServerQueue queue;

    GameServerInfo info;
    info.address = "srv";
    info.status  = GameServerStatus::offline;
    queue.register_server(info);

    queue.update_heartbeat("srv");

    auto found = queue.find_server("srv");
    REQUIRE(found);
    CHECK(found.value().status == GameServerStatus::online);
}

} // namespace pvpgn::application::realm
