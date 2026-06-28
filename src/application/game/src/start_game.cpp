// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/start_game.hpp"

#include "domain/gameplay/ports.hpp"

namespace pvpgn::application::game {

core::Result<StartGameResult, StartGameError>
StartGame::execute(domain::AccountId host_account_id, domain::ClientTag client_tag,
                   const std::string& game_name, const std::string& map_name,
                   std::uint8_t max_players, std::uint16_t gametype) const {
    // 1. Validate game parameters
    if (game_name.empty()) {
        return core::fail(StartGameError::InvalidGameName);
    }
    if (max_players == 0 || max_players > 16) {
        return core::fail(StartGameError::MaxPlayersOutOfRange);
    }

    // 2. Create new game aggregate
    domain::gameplay::GameDescriptor desc{
        .name = game_name,
        .map = map_name,
        .max_players = max_players,
        .gametype = gametype,
    };

    // Use a simple ID generation strategy (in real impl, would use ID service)
    domain::GameId game_id{static_cast<std::uint32_t>(std::time(nullptr))};

    auto game_result = domain::gameplay::Game::host(game_id, host_account_id, client_tag, desc);
    if (!game_result) {
        return core::fail(StartGameError::InvalidGameName);
    }

    domain::gameplay::Game game = game_result.value();

    // 3. Save to repository
    auto save_result = game_repo_.save(game);
    if (!save_result) {
        return core::fail(StartGameError::InvalidGameName);
    }

    // 4. Drain domain events
    auto events = game.drain_events();
    (void)events;  // Events will be processed by caller

    // 5. Return result with placeholder encoded entry
    return StartGameResult{
        .game = game,
        .game_id = game_id,
        .encoded_game_entry = "",  // Infrastructure layer encodes this
    };
}

}  // namespace pvpgn::application::game
