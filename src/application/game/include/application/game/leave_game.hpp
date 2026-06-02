// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file leave_game.hpp
/// LEAVE_GAME use-case — client leaves an active game.
///
/// Removes a player from the game. If the host leaves, either migrates
/// host to another player or closes the game. If the game becomes empty,
/// it is deleted from the repository.

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/ids.hpp"
#include "domain/gameplay/ports.hpp"

namespace pvpgn::application::game {

/// Errors that can occur during game leave.
enum class LeaveGameError : std::uint8_t {
    GameNotFound,
    NotInGame,
};

/// Result of a successful game leave.
struct LeaveGameResult {
    /// Whether the game was deleted (empty + host left).
    bool game_deleted = false;
    /// Whether host was migrated to another player.
    bool host_migrated = false;
    /// New host if migration occurred (otherwise invalid).
    domain::AccountId new_host = domain::AccountId{0};
};

class LeaveGame {
public:
    explicit LeaveGame(domain::gameplay::IGameRepository& game_repo)
        : game_repo_(game_repo) {}

    /// Execute: remove account from the given game.
    /// Handles host migration if the host leaves.
    core::Result<LeaveGameResult, LeaveGameError>
    execute(domain::GameId game_id, domain::AccountId account_id) const;

private:
    domain::gameplay::IGameRepository& game_repo_;
};

}  // namespace pvpgn::application::game
