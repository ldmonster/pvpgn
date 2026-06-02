// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file start_game.hpp
/// START_GAME use-case — host creates and starts a new game.
///
/// Checks that the account is not already in a game, creates a Game
/// aggregate, saves it to the repository, and returns the game ID.
/// Returns the encoded game entry for the client list.

#include <string>

#include "domain/gameplay/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::game {

/// Errors that can occur during game start.
enum class StartGameError : std::uint8_t {
    AlreadyInGame,
    InvalidGameName,
    InvalidGameFlags,
    MaxPlayersOutOfRange,
};

/// Result of a successful game start.
struct StartGameResult {
    /// The newly created game.
    domain::gameplay::Game game;
    /// Game ID for the client.
    domain::GameId game_id;
    /// Encoded game entry (populated by infrastructure layer).
    std::string encoded_game_entry;
};

class StartGame {
public:
    explicit StartGame(domain::gameplay::IGameRepository& game_repo)
        : game_repo_(game_repo) {}

    /// Execute: create a new game with the given parameters.
    core::Result<StartGameResult, StartGameError>
    execute(domain::AccountId host_account_id, domain::ClientTag client_tag,
            const std::string& game_name, const std::string& map_name,
            std::uint8_t max_players) const;

private:
    domain::gameplay::IGameRepository& game_repo_;
};

}  // namespace pvpgn::application::game
