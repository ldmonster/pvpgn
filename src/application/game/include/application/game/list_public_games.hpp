// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file list_public_games.hpp
/// LIST_PUBLIC_GAMES use-case — enumerate non-private games.
///
/// Returns a paginated list of games that are:
///   - Not private (no password set)
///   - Optionally filtered by client tag or game type
///   - Ordered by creation time (newest first)

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/gameplay/ports.hpp"

namespace pvpgn::application::game {

struct GameInfo {
    domain::GameId            id;
    std::string              name;
    std::size_t              current_players;
    std::size_t              max_players;
    std::string              game_type;
    std::string              map_name;
    bool                     is_private;
    /// Lifecycle state of the hosted match. The protocol layer maps this (plus
    /// the player counts) to the SID_GETADVLISTEX status word — an in-progress
    /// or finished game must not be advertised as "open".
    domain::gameplay::GameState state = domain::gameplay::GameState::Open;
};

struct ListPublicGamesRequest {
    std::optional<domain::ClientTag> filter_by_tag;
    std::optional<std::string>       filter_by_type;
    std::uint32_t                    max_results{50};
};

enum class ListPublicGamesError : std::uint8_t {
    InvalidRequest,
    RepositoryError,
};

class ListPublicGames {
public:
    explicit ListPublicGames(std::shared_ptr<domain::gameplay::IGameRepository> games)
        : games_(games) {}

    core::Result<std::vector<GameInfo>, ListPublicGamesError>
    execute(const ListPublicGamesRequest& req);

private:
    std::shared_ptr<domain::gameplay::IGameRepository> games_;
};

}  // namespace pvpgn::application::game
