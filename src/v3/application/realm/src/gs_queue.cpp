#include "application/realm/gs_queue.hpp"

namespace pvpgn::application::realm {

void GameServerQueue::register_server(GameServerInfo info) {
    std::lock_guard<std::mutex> lock(mutex_);
    servers_[info.address] = std::move(info);
}

void GameServerQueue::update_heartbeat(std::string_view address) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = servers_.find(std::string(address));
    if (it != servers_.end()) {
        it->second.last_heartbeat = std::chrono::system_clock::now();
        it->second.status = GameServerStatus::online;
    }
}

void GameServerQueue::remove_server(std::string_view address) {
    std::lock_guard<std::mutex> lock(mutex_);
    servers_.erase(std::string(address));
}

core::Result<uint32_t, core::Error> GameServerQueue::create_game(GameInfo info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint32_t token = next_token_++;
    info.game_token = token;
    info.created_at = std::chrono::system_clock::now();
    
    games_[info.game_name] = info;
    return core::Result<uint32_t, core::Error>(token);
}

core::Result<GameInfo, core::Error> GameServerQueue::find_game(std::string_view game_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = games_.find(std::string(game_name));
    if (it == games_.end()) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "Game not found: " + std::string(game_name))
        );
    }
    
    return core::Result<GameInfo, core::Error>(it->second);
}

core::Result<void, core::Error> GameServerQueue::join_game(std::string_view game_name, std::string_view player) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = games_.find(std::string(game_name));
    if (it == games_.end()) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "Game not found: " + std::string(game_name))
        );
    }
    
    it->second.players.push_back(std::string(player));
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> GameServerQueue::leave_game(std::string_view game_name, std::string_view player) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = games_.find(std::string(game_name));
    if (it == games_.end()) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "Game not found: " + std::string(game_name))
        );
    }
    
    auto& players = it->second.players;
    auto player_it = std::find(players.begin(), players.end(), player);
    if (player_it != players.end()) {
        players.erase(player_it);
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> GameServerQueue::close_game(std::string_view game_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = games_.find(std::string(game_name));
    if (it == games_.end()) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "Game not found: " + std::string(game_name))
        );
    }
    
    games_.erase(it);
    return core::Result<void, core::Error>();
}

std::vector<GameInfo> GameServerQueue::list_games(bool expansion_only) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<GameInfo> result;
    for (const auto& [name, info] : games_) {
        if (!expansion_only || info.is_expansion) {
            result.push_back(info);
        }
    }
    return result;
}

std::vector<GameServerInfo> GameServerQueue::list_servers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<GameServerInfo> result;
    for (const auto& [addr, info] : servers_) {
        result.push_back(info);
    }
    return result;
}

core::Result<GameServerInfo, core::Error> GameServerQueue::find_server(std::string_view address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = servers_.find(std::string(address));
    if (it == servers_.end()) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "Server not found: " + std::string(address))
        );
    }
    
    return core::Result<GameServerInfo, core::Error>(it->second);
}

core::Result<GameServerInfo, core::Error> GameServerQueue::select_server(bool expansion) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    GameServerInfo best_server;
    bool found = false;
    uint32_t best_load = std::numeric_limits<uint32_t>::max();
    
    for (const auto& [addr, info] : servers_) {
        if (info.status != GameServerStatus::online) {
            continue;
        }
        if (expansion && !info.is_expansion) {
            continue;
        }
        if (info.current_players >= info.max_players) {
            continue;
        }
        
        uint32_t load = info.current_players;
        if (load < best_load) {
            best_load = load;
            best_server = info;
            found = true;
        }
    }
    
    if (!found) {
        return core::fail(
            core::make_error(core::StatusCode::Unavailable, "No available game servers")
        );
    }
    
    return core::Result<GameServerInfo, core::Error>(best_server);
}

void GameServerQueue::cleanup_stale(std::chrono::seconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    
    std::vector<std::string> to_remove;
    for (auto& [addr, info] : servers_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - info.last_heartbeat
        );
        if (elapsed > timeout) {
            to_remove.push_back(addr);
        }
    }
    
    for (const auto& addr : to_remove) {
        servers_.erase(addr);
    }
}

} // namespace pvpgn::application::realm
