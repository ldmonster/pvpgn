// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file cancel_game.hpp
/// CANCEL_GAME use-case — game owner cancels an open game.
///
/// Removes the game from the repository entirely. Only the game owner
/// (host) is permitted to cancel. Returns PermissionDenied if the
/// requester is not the host, or GameNotFound if the game does not exist.

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/gameplay/ports.hpp"

namespace pvpgn::application::game {

/// Errors that can occur during game cancellation.
enum class CancelGameError : std::uint8_t {
    GameNotFound,
    PermissionDenied,
};

/// Command object for the CancelGame use-case.
struct CancelGameCommand {
    domain::GameId    game_id;
    domain::AccountId requester_id;  ///< Must be the game owner (host).
};

class CancelGame {
public:
    explicit CancelGame(domain::gameplay::IGameRepository& games)
        : games_(games) {}

    /// Execute: cancel the game identified by cmd.game_id.
    /// @returns GameNotFound  if no game with that ID exists.
    /// @returns PermissionDenied if requester_id is not the game host.
    [[nodiscard]] core::Result<void, CancelGameError>
    execute(CancelGameCommand cmd) const;

private:
    domain::gameplay::IGameRepository& games_;
};

}  // namespace pvpgn::application::game
