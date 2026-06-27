#pragma once
#include "core/result.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace pvpgn::application::realm {

enum class GameServerStatus { online, offline };

struct GameServerInfo {
    std::string address;
    uint16_t port = 4000;
    GameServerStatus status = GameServerStatus::offline;
    uint32_t current_players = 0;
    uint32_t max_players = 255;
    std::chrono::system_clock::time_point last_heartbeat;
    std::string version;
    bool is_expansion = false;
};

struct GameInfo {
    std::string game_name;
    std::string game_password;
    std::string description;
    std::string gs_address;
    uint16_t gs_port = 0;
    uint32_t game_token = 0;
    uint8_t difficulty = 0;
    bool is_expansion = false;
    std::chrono::system_clock::time_point created_at;
    std::vector<std::string> players;
};

class GameServerQueue {
public:
    // Register/update a game server
    void register_server(GameServerInfo info);
    void update_heartbeat(std::string_view address);
    void remove_server(std::string_view address);
    
    // Game management
    core::Result<uint32_t, core::Error> create_game(GameInfo info);
    core::Result<GameInfo, core::Error> find_game(std::string_view game_name);
    core::Result<void, core::Error> join_game(std::string_view game_name, std::string_view player);
    core::Result<void, core::Error> leave_game(std::string_view game_name, std::string_view player);
    core::Result<void, core::Error> close_game(std::string_view game_name);
    
    // Query
    std::vector<GameInfo> list_games(bool expansion_only = false) const;
    std::vector<GameServerInfo> list_servers() const;
    core::Result<GameServerInfo, core::Error> find_server(std::string_view address) const;
    
    // Select best available server for new game
    core::Result<GameServerInfo, core::Error> select_server(bool expansion) const;
    
    // Cleanup stale entries
    void cleanup_stale(std::chrono::seconds timeout = std::chrono::seconds{60});

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, GameServerInfo> servers_;
    std::unordered_map<std::string, GameInfo> games_;
    uint32_t next_token_ = 1;
};

} // namespace pvpgn::application::realm
