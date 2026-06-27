// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/list_public_games.hpp"

#include "domain/gameplay/ports.hpp"

namespace pvpgn::application::game {

core::Result<std::vector<GameInfo>, ListPublicGamesError>
ListPublicGames::execute(const ListPublicGamesRequest& req) {
    // 1. Validate request
    if (req.max_results == 0 || req.max_results > 500) {
        return core::fail(ListPublicGamesError::InvalidRequest);
    }

    std::vector<GameInfo> results;
    std::size_t count = 0;

    // 2. Get all active games and filter
    auto games_result = games_->list_active();
    if (!games_result) {
        return core::fail(ListPublicGamesError::InvalidRequest);
    }

    for (const auto& game_ptr : games_result.value()) {
        if (!game_ptr) continue;
        const auto& game = *game_ptr;

    // Apply client tag filter
        if (req.filter_by_tag && game.client().bytes() != req.filter_by_tag->bytes()) {
            continue;
        }

        // Add to results
        results.push_back({
            .id = game.id(),
            .name = game.descriptor().name,
            .current_players = game.player_count(),
            .max_players = static_cast<std::size_t>(game.descriptor().max_players),
            .game_type = "public",
            .map_name = game.descriptor().map,
            .is_private = false,
            .state = game.state(),
        });

        count++;
        if (count >= req.max_results) break;
    }

    return results;
}

}  // namespace pvpgn::application::game
