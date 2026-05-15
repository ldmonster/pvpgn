// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_repository.hpp
/// Thread-safe in-memory store for games. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "application/ports/game_repository.hpp"

namespace pvpgn::infra::storage {

class InMemoryGameRepository final
    : public application::ports::IGameRepository {
public:
    core::Result<domain::gameplay::Game>
    find_by_id(domain::GameId id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "game: id not found"});
        }
        return *it->second;
    }

    core::Status<>
    save(const domain::gameplay::Game& game) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_unique<domain::gameplay::Game>(game);
        by_id_[game.id().value()] = std::move(copy);
        return core::ok();
    }

    core::Status<> remove(domain::GameId id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "game: id not found"});
        }
        by_id_.erase(it);
        return core::ok();
    }

    void forEach(std::function<bool(const domain::gameplay::Game&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [id, game] : by_id_) {
            if (!predicate(*game)) break;
        }
    }

    std::size_t size() const noexcept override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return by_id_.size();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<domain::gameplay::Game>> by_id_;
};

}  // namespace pvpgn::infra::storage
