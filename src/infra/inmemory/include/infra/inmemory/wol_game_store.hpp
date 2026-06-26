// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_game_store.hpp
/// In-memory IWolGameStore — open WOL game-channels keyed by game name
/// (case-insensitive). Thread-safe; used by the inmemory bnetd backend and by
/// tests. Mirrors [[wol_credential_store]].

#include <algorithm>
#include <cctype>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "application/game/wol_game_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryWolGameStore final : public application::game::IWolGameStore {
public:
    void create(const application::game::WolGameInfo& game) override {
        std::unique_lock lock(mutex_);
        by_name_[key(game.name)] = game;
    }

    [[nodiscard]] std::optional<application::game::WolGameInfo>
    find(std::string_view name) const override {
        std::shared_lock lock(mutex_);
        auto it = by_name_.find(key(name));
        if (it == by_name_.end()) return std::nullopt;
        return it->second;
    }

    bool add_player(std::string_view name, domain::AccountId player) override {
        std::unique_lock lock(mutex_);
        auto it = by_name_.find(key(name));
        if (it == by_name_.end()) return false;
        auto& players = it->second.players;
        if (std::find(players.begin(), players.end(), player) != players.end()) {
            return true;  // already a member
        }
        if (it->second.is_full()) return false;
        players.push_back(player);
        return true;
    }

    void remove(std::string_view name) override {
        std::unique_lock lock(mutex_);
        by_name_.erase(key(name));
    }

private:
    static std::string key(std::string_view name) {
        std::string k{name};
        std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return k;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, application::game::WolGameInfo> by_name_;
};

}  // namespace pvpgn::infra::inmemory
