// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_repository.hpp
/// Thread-safe in-memory store for games. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "application/ports/game_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryGameRepository final
    : public application::ports::IGameRepository {
public:
    core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
    find_by_name(std::string_view name) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_name_.find(std::string(name));
        if (it == by_name_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "game: name not found"});
        }
        return it->second;
    }

    core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
    find_by_id(uint32_t id) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "game: id not found"});
        }
        return it->second;
    }

    core::Result<void, core::Error>
    save(const domain::gameplay::Game& game) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_shared<domain::gameplay::Game>(game);
        by_id_[game.id().value()] = copy;
        by_name_[game.descriptor().name] = copy;
        return core::ok();
    }

    core::Result<void, core::Error>
    remove(std::string_view name) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_name_.find(std::string(name));
        if (it == by_name_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "game: name not found"});
        }
        by_id_.erase(it->second->id().value());
        by_name_.erase(it);
        return core::ok();
    }

    core::Result<std::vector<std::shared_ptr<domain::gameplay::Game>>, core::Error>
    list_active() override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::shared_ptr<domain::gameplay::Game>> result;
        for (const auto& [id, game] : by_id_) {
            result.push_back(game);
        }
        return result;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::shared_ptr<domain::gameplay::Game>> by_id_;
    std::unordered_map<std::string,
                       std::shared_ptr<domain::gameplay::Game>> by_name_;
};

}  // namespace pvpgn::infra::inmemory
