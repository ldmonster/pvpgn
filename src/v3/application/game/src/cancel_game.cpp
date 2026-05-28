// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/cancel_game.hpp"

#include "application/ports/game_repository.hpp"

namespace pvpgn::application::game {

core::Result<void, CancelGameError>
CancelGame::execute(CancelGameCommand cmd) const {
    // 1. Find the game by ID
    auto found = games_.find_by_id(cmd.game_id.value());
    if (!found) {
        return core::fail(CancelGameError::GameNotFound);
    }

    auto game = found.value();

    // 2. Verify requester is the game host
    if (cmd.requester_id.value() != game->host().value()) {
        return core::fail(CancelGameError::PermissionDenied);
    }

    // 3. Remove the game from the repository (keyed by name)
    auto remove_result = games_.remove(game->descriptor().name);
    if (!remove_result) {
        return core::fail(CancelGameError::GameNotFound);
    }

    return core::Result<void, CancelGameError>{};
}

}  // namespace pvpgn::application::game
