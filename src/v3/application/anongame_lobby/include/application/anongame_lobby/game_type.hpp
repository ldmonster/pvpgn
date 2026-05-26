// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_type.hpp
/// Pure-C++ mirror of the legacy `_anongame_totalplayers` lookup
/// table from `src/bnetd/anongame.cpp`. Keeps the bracket-size
/// policy out of the legacy state machine so the future
/// `IAnonGameLobbyRepository` adapter can implement
/// `bracket_size_for(game_type)` against this table instead of
/// reaching into the file-static legacy `players[][]` array.
///
/// The numeric values come from
/// `src/common/anongame_protocol.h` `ANONGAME_TYPE_*` -- the
/// constants below intentionally repeat them so application/ does
/// NOT depend on the legacy header (layering rule: application
/// must not depend on infra or legacy).

#include <cstdint>

namespace pvpgn::application::anongame_lobby {

/// `ANONGAME_TYPE_*` numeric values copied from
/// `src/common/anongame_protocol.h` so the application layer does
/// not include any legacy headers.
enum class GameType : std::uint32_t {
    k1v1         = 0,
    k2v2         = 1,
    k3v3         = 2,
    k4v4         = 3,
    kSmallFFA    = 4,
    kAT2v2       = 5,
    kTeamFFA     = 6,  ///< legacy: no longer supported
    kAT3v3       = 7,
    kAT4v4       = 8,
    kTournament  = 9,  ///< total-players is dynamic -- caller resolves
    k5v5         = 10,
    k6v6         = 11,
    k2v2v2       = 12,
    k3v3v3       = 13,
    k4v4v4       = 14,
    k2v2v2v2     = 15,
    k3v3v3v3     = 16,
    kAT2v2v2     = 17,
};

/// Returns the total number of players needed to fill the
/// matchmaking bracket for `game_type`. Mirrors legacy
/// `_anongame_totalplayers` in `src/bnetd/anongame.cpp`.
///
/// Returns 0 for:
///   - unknown / out-of-range game_type values,
///   - `kTournament` (dynamic via legacy `tournament_get_totalplayers()`;
///     the legacy adapter is expected to override this special case).
constexpr std::uint8_t bracket_size_for_game_type(std::uint32_t game_type) noexcept {
    switch (static_cast<GameType>(game_type)) {
    case GameType::k1v1:         return 2;
    case GameType::k2v2:
    case GameType::kAT2v2:
    case GameType::kSmallFFA:    return 4;
    case GameType::k3v3:
    case GameType::kAT3v3:
    case GameType::k2v2v2:
    case GameType::kAT2v2v2:     return 6;
    case GameType::k4v4:
    case GameType::kAT4v4:
    case GameType::kTeamFFA:
    case GameType::k2v2v2v2:     return 8;
    case GameType::k3v3v3:       return 9;
    case GameType::k5v5:         return 10;
    case GameType::k6v6:
    case GameType::k4v4v4:
    case GameType::k3v3v3v3:     return 12;
    case GameType::kTournament:  return 0;  // dynamic; legacy adapter overrides
    }
    return 0;  // unknown
}

}  // namespace pvpgn::application::anongame_lobby
