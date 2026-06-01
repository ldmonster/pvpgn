// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file join_game.hpp
/// JOIN_GAME use-case — client joins an existing game.
///
/// Validates that the game exists, is open, not full, and the account
/// is not already in another game. Adds the player to the game.
/// Returns game server address and port.

#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/ids.hpp"
#include "application/ports/ports.hpp"

namespace pvpgn::application::game {

/// Errors that can occur during game join.
enum class JoinGameError : std::uint8_t {
    GameNotFound,
    GameFull,
    GameClosed,
    AlreadyInGame,
    WrongClientTag,
};

/// Result of a successful game join.
struct JoinGameResult {
    /// The updated game snapshot.
    domain::gameplay::Game game;
    /// Game server address for connection.
    std::string server_address;
    /// Game server port for connection.
    std::uint16_t server_port = 0;
};

class JoinGame {
public:
    explicit JoinGame(application::ports::IGameRepository& game_repo)
        : game_repo_(game_repo) {}

    /// Execute: add account to the given game.
    core::Result<JoinGameResult, JoinGameError>
    execute(domain::GameId game_id, domain::AccountId account_id) const;

private:
    application::ports::IGameRepository& game_repo_;
};

}  // namespace pvpgn::application::game
