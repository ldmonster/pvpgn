// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_repository.hpp
/// Persistence port for the `gameplay::Game` aggregate.
/// (Forward-declared to avoid circular dependencies)

#include <cstddef>
#include <functional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::gameplay {
class Game;
}

namespace pvpgn::application::ports {

class IGameRepository {
public:
    virtual ~IGameRepository() = default;

    /// Look up a game by primary key.
    virtual core::Result<domain::gameplay::Game>
    find_by_id(domain::GameId id) const = 0;

    /// Find a game by name.
    virtual core::Result<domain::gameplay::Game>
    find_by_name(std::string_view name) const = 0;

    /// Upsert. Implementations are expected to be idempotent.
    virtual core::Status<>
    save(const domain::gameplay::Game& game) = 0;

    /// Remove a game.
    virtual core::Status<>
    remove(domain::GameId id) = 0;

    /// Iterate over all games, applying predicate. Early exit on
    /// predicate returning false.
    virtual void
    forEach(std::function<bool(domain::gameplay::Game&)> predicate) = 0;

    virtual std::size_t size() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
