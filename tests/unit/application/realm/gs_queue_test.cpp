#include <catch2/catch_test_macros.hpp>
#include "application/realm/gs_queue.hpp"
#include <chrono>
#include <thread>

namespace pvpgn::application::realm {

TEST_CASE("GameServerQueue - register and find server", "[application][realm]") {
    GameServerQueue queue;
    
    GameServerInfo info;
    info.address = "gs1.example.com";
    info.port = 4000;
    info.status = GameServerStatus::online;
    info.current_players = 10;
    info.max_players = 255;
    
    queue.register_server(info);
    
    auto result = queue.find_server("gs1.example.com");
    REQUIRE(result);
    CHECK(result.value().address == "gs1.example.com");
    CHECK(result.value().port == 4000);
    CHECK(result.value().status == GameServerStatus::online);
}

TEST_CASE("GameServerQueue - find non-existent server fails", "[application][realm]") {
    GameServerQueue queue;
    
    auto result = queue.find_server("nonexistent.example.com");
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - list servers", "[application][realm]") {
    GameServerQueue queue;
    
    GameServerInfo info1;
    info1.address = "gs1.example.com";
    info1.port = 4000;
    
    GameServerInfo info2;
    info2.address = "gs2.example.com";
    info2.port = 4001;
    
    queue.register_server(info1);
    queue.register_server(info2);
    
    auto servers = queue.list_servers();
    CHECK(servers.size() == 2);
}

TEST_CASE("GameServerQueue - remove server", "[application][realm]") {
    GameServerQueue queue;
    
    GameServerInfo info;
    info.address = "gs1.example.com";
    info.port = 4000;
    
    queue.register_server(info);
    queue.remove_server("gs1.example.com");
    
    auto result = queue.find_server("gs1.example.com");
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - update heartbeat", "[application][realm]") {
    GameServerQueue queue;
    
    GameServerInfo info;
    info.address = "gs1.example.com";
    info.port = 4000;
    info.status = GameServerStatus::offline;
    
    queue.register_server(info);
    queue.update_heartbeat("gs1.example.com");
    
    auto result = queue.find_server("gs1.example.com");
    REQUIRE(result);
    CHECK(result.value().status == GameServerStatus::online);
}

TEST_CASE("GameServerQueue - create game", "[application][realm]") {
    GameServerQueue queue;
    
    GameInfo info;
    info.game_name = "TestGame";
    info.game_password = "password";
    info.description = "Test game";
    info.gs_address = "gs1.example.com";
    info.gs_port = 4000;
    info.difficulty = 0;
    info.is_expansion = false;
    
    auto result = queue.create_game(info);
    REQUIRE(result);
    CHECK(result.value() > 0);  // Token should be positive
}

TEST_CASE("GameServerQueue - find game", "[application][realm]") {
    GameServerQueue queue;
    
    GameInfo info;
    info.game_name = "TestGame";
    info.game_password = "password";
    info.description = "Test game";
    info.gs_address = "gs1.example.com";
    info.gs_port = 4000;
    
    auto create_result = queue.create_game(info);
    REQUIRE(create_result);
    
    auto result = queue.find_game("TestGame");
    REQUIRE(result);
    CHECK(result.value().game_name == "TestGame");
    CHECK(result.value().game_password == "password");
}

TEST_CASE("GameServerQueue - find non-existent game fails", "[application][realm]") {
    GameServerQueue queue;
    
    auto result = queue.find_game("NonExistent");
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - join game", "[application][realm]") {
    GameServerQueue queue;
    
    GameInfo info;
    info.game_name = "TestGame";
    info.game_password = "password";
    info.description = "Test game";
    info.gs_address = "gs1.example.com";
    info.gs_port = 4000;
    
    auto create_result = queue.create_game(info);
    REQUIRE(create_result);
    
    auto join_result = queue.join_game("TestGame", "player1");
    REQUIRE(join_result);
    
    auto find_result = queue.find_game("TestGame");
    REQUIRE(find_result);
    auto game = std::move(find_result).value();
    CHECK(game.players.size() == 1);
    CHECK(game.players[0] == "player1");
}

TEST_CASE("GameServerQueue - join non-existent game fails", "[application][realm]") {
    GameServerQueue queue;
    
    auto result = queue.join_game("NonExistent", "player1");
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - leave game", "[application][realm]") {
    GameServerQueue queue;
    
    GameInfo info;
    info.game_name = "TestGame";
    info.game_password = "password";
    info.description = "Test game";
    info.gs_address = "gs1.example.com";
    info.gs_port = 4000;
    
    auto create_result = queue.create_game(info);
    REQUIRE(create_result);
    auto join1 = queue.join_game("TestGame", "player1");
    REQUIRE(join1);
    auto join2 = queue.join_game("TestGame", "player2");
    REQUIRE(join2);
    
    auto leave_result = queue.leave_game("TestGame", "player1");
    REQUIRE(leave_result);
    
    auto find_result = queue.find_game("TestGame");
    REQUIRE(find_result);
    auto game = std::move(find_result).value();
    CHECK(game.players.size() == 1);
    CHECK(game.players[0] == "player2");
}

TEST_CASE("GameServerQueue - close game", "[application][realm]") {
    GameServerQueue queue;
    
    GameInfo info;
    info.game_name = "TestGame";
    info.game_password = "password";
    info.description = "Test game";
    info.gs_address = "gs1.example.com";
    info.gs_port = 4000;
    
    auto create_result = queue.create_game(info);
    REQUIRE(create_result);
    
    auto close_result = queue.close_game("TestGame");
    REQUIRE(close_result);
    
    auto find_result = queue.find_game("TestGame");
    CHECK_FALSE(find_result);
}

TEST_CASE("GameServerQueue - list games", "[application][realm]") {
    GameServerQueue queue;
    
    GameInfo info1;
    info1.game_name = "Game1";
    info1.is_expansion = false;
    
    GameInfo info2;
    info2.game_name = "Game2";
    info2.is_expansion = true;
    
    auto create1 = queue.create_game(info1);
    REQUIRE(create1);
    auto create2 = queue.create_game(info2);
    REQUIRE(create2);
    
    auto all_games = queue.list_games(false);
    CHECK(all_games.size() == 2);
    
    auto expansion_games = queue.list_games(true);
    CHECK(expansion_games.size() == 1);
    CHECK(expansion_games[0].game_name == "Game2");
}

TEST_CASE("GameServerQueue - select server", "[application][realm]") {
    GameServerQueue queue;
    
    GameServerInfo info;
    info.address = "gs1.example.com";
    info.port = 4000;
    info.status = GameServerStatus::online;
    info.is_expansion = true;
    
    queue.register_server(info);
    
    auto result = queue.select_server(true);
    REQUIRE(result);
    CHECK(result.value().address == "gs1.example.com");
}

TEST_CASE("GameServerQueue - select server with no available servers fails", "[application][realm]") {
    GameServerQueue queue;
    
    auto result = queue.select_server(true);
    CHECK_FALSE(result);
}

TEST_CASE("GameServerQueue - game token increments", "[application][realm]") {
    GameServerQueue queue;
    
    GameInfo info1;
    info1.game_name = "Game1";
    
    GameInfo info2;
    info2.game_name = "Game2";
    
    auto token1 = queue.create_game(info1);
    auto token2 = queue.create_game(info2);
    
    REQUIRE(token1);
    REQUIRE(token2);
    CHECK(token2.value() > token1.value());
}

} // namespace pvpgn::application::realm
