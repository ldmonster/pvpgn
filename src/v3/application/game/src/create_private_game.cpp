// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/create_private_game.hpp"

#include <ctime>

#include "application/ports/event_bus.hpp"
#include "application/ports/game_repository.hpp"
#include "domain/gameplay/game.hpp"

namespace pvpgn::application::game {

core::Result<domain::GameId, CreatePrivateGameError>
CreatePrivateGame::execute(const CreatePrivateGameRequest& req) {
    // 1. Validate input
    if (req.game_name.empty()) {
        return core::fail(CreatePrivateGameError::InvalidGameName);
    }
    if (req.password.empty()) {
        return core::fail(CreatePrivateGameError::InvalidPassword);
    }
    if (req.max_players == 0 || req.max_players > 16) {
        return core::fail(CreatePrivateGameError::MaxPlayersOutOfRange);
    }

    // 2. Create new game aggregate
    domain::gameplay::GameDescriptor desc{
        .name = req.game_name,
        .map = req.map_name,
        .password = req.password,
        .max_players = static_cast<std::uint8_t>(req.max_players),
    };

    // Generate game ID (simple strategy: timestamp-based)
    domain::GameId game_id{static_cast<std::uint32_t>(std::time(nullptr))};

    // 3. Host the game
    auto game_result = domain::gameplay::Game::host(
        game_id, req.host_id, req.client_tag, desc);
    if (!game_result) {
        return core::fail(CreatePrivateGameError::InvalidGameName);
    }

    domain::gameplay::Game game = game_result.value();

    // 4. Start the game immediately for private games
    auto start_outcome = game.start(req.host_id, core::SystemTime::now());
    if (start_outcome == domain::gameplay::Game::StartOutcome::WrongState) {
        return core::fail(CreatePrivateGameError::Internal);
    }

    // 5. Save to repository
    auto save_result = games_->save(game);
    if (!save_result) {
        return core::fail(CreatePrivateGameError::PersistenceFailed);
    }

    // 6. Drain and publish events
    auto events = game.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return game_id;
}

}  // namespace pvpgn::application::game
