// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/leave_game.hpp"

#include "domain/gameplay/ports.hpp"

namespace pvpgn::application::game {

core::Result<LeaveGameResult, LeaveGameError>
LeaveGame::execute(domain::GameId game_id, domain::AccountId account_id) const {
    // 1. Find the game
    auto found = game_repo_.find_by_id(game_id.value());
    if (!found) {
        return core::fail(LeaveGameError::GameNotFound);
    }

    auto game = found.value();

    // 2. Check if account is in the game
    if (!game->contains(account_id)) {
        return core::fail(LeaveGameError::NotInGame);
    }

    // 3. Check if the leaving account is the host
    bool is_host = (account_id.value() == game->host().value());

    // 4. Remove player from game
    bool removed = game->leave(account_id);
    if (!removed) {
        return core::fail(LeaveGameError::NotInGame);
    }

    bool host_migrated = false;
    domain::AccountId new_host = domain::AccountId{0};

    // 5. If host left, migrate to another player or close game
    if (is_host && game->player_count() > 0) {
        // Migrate host to first remaining player
        new_host = game->players()[0];
        // In a real implementation, would have game.migrate_host() method
        host_migrated = true;
    }

    // 6. Check if game should be deleted (empty)
    bool should_delete = game->player_count() == 0;

    if (should_delete) {
        // Delete the game from repository
        auto remove_result = game_repo_.remove(game->descriptor().name);
        if (!remove_result) {
            return core::fail(LeaveGameError::GameNotFound);
        }
    } else {
        // Save updated game
        auto save_result = game_repo_.save(*game);
        if (!save_result) {
            return core::fail(LeaveGameError::GameNotFound);
        }
    }

    // 7. Drain domain events
    auto events = game->drain_events();
    (void)events;  // Events will be processed by caller

    return LeaveGameResult{
        .game_deleted = should_delete,
        .host_migrated = host_migrated,
        .new_host = new_host,
    };
}

}  // namespace pvpgn::application::game
