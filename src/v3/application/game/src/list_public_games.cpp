// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/list_public_games.hpp"

#include "application/ports/game_repository.hpp"

namespace pvpgn::application::game {

core::Result<std::vector<GameInfo>, ListPublicGamesError>
ListPublicGames::execute(const ListPublicGamesRequest& req) {
    // 1. Validate request
    if (req.max_results == 0 || req.max_results > 500) {
        return core::fail(ListPublicGamesError::InvalidRequest);
    }

    std::vector<GameInfo> results;
    std::size_t count = 0;

    // 2. Iterate through all games and filter
    games_->forEach([&](domain::gameplay::Game& game) -> bool {
        // Skip private games
        if (!game.descriptor().password.empty()) {
            return true;  // Continue iteration
        }

        // Apply client tag filter
        if (req.filter_by_tag && game.client() != *req.filter_by_tag) {
            return true;
        }

        // Apply game type filter
        if (req.filter_by_type && game.descriptor().type != *req.filter_by_type) {
            return true;
        }

        // Add to results
        results.push_back({
            .id = game.id(),
            .name = game.descriptor().name,
            .current_players = game.player_count(),
            .max_players = static_cast<std::size_t>(game.descriptor().max_players),
            .game_type = game.descriptor().type,
            .map_name = game.descriptor().map,
            .is_private = false,
        });

        count++;
        return count < req.max_results;  // Stop if we've reached max
    });

    return results;
}

}  // namespace pvpgn::application::game
