// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/join_game.hpp"

#include "application/ports/game_repository.hpp"

namespace pvpgn::application::game {

core::Result<JoinGameResult, JoinGameError>
JoinGame::execute(domain::GameId game_id, domain::AccountId account_id) const {
    // 1. Find the game
    auto found = game_repo_.find_by_id(game_id);
    if (!found) {
        return core::fail(JoinGameError::GameNotFound);
    }

    domain::gameplay::Game game = found.value();

    // 2. Attempt to join
    auto outcome = game.join(account_id);

    // Map domain outcome to application error
    switch (outcome) {
        case domain::gameplay::Game::JoinOutcome::Closed:
            return core::fail(JoinGameError::GameClosed);
        case domain::gameplay::Game::JoinOutcome::AlreadyIn:
            return core::fail(JoinGameError::AlreadyInGame);
        case domain::gameplay::Game::JoinOutcome::Full:
            return core::fail(JoinGameError::GameFull);
        case domain::gameplay::Game::JoinOutcome::Joined:
            break;
    }

    // 3. Save updated game
    auto save_result = game_repo_.save(game);
    if (!save_result) {
        return core::fail(JoinGameError::GameNotFound);
    }

    // 4. Drain domain events
    auto events = game.drain_events();
    (void)events;  // Events will be processed by caller

    // 5. Return result with server address/port (placeholder)
    return JoinGameResult{
        .game = game,
        .server_address = "127.0.0.1",  // Infrastructure layer populates real address
        .server_port = 0,                 // Infrastructure layer populates real port
    };
}

}  // namespace pvpgn::application::game
