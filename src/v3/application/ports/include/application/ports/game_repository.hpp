// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/result.hpp"
#include "domain/gameplay/game.hpp"
#include <string>
#include <vector>
#include <memory>

namespace pvpgn::domain::gameplay {
// Game is now fully defined via include above
}

namespace pvpgn::application::ports {

/// Port: Game repository interface for hexagonal architecture.
class IGameRepository {
public:
    virtual ~IGameRepository() = default;

    /// Find game by name.
    virtual core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
        find_by_name(std::string_view name) = 0;

    /// Find game by ID.
    virtual core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
        find_by_id(uint32_t id) = 0;

    /// Save or update a game.
    virtual core::Result<void, core::Error>
        save(const domain::gameplay::Game& game) = 0;

    /// Remove game by name.
    virtual core::Result<void, core::Error>
        remove(std::string_view name) = 0;

    /// List all active (in-progress) games.
    virtual core::Result<std::vector<std::shared_ptr<domain::gameplay::Game>>, core::Error>
        list_active() = 0;
};

} // namespace pvpgn::application::ports
