// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tournament-aware TYPE decorator.
//
// The legacy bnetd code (`bnetd/anongame_infos.cpp`, the TY section)
// re-writes the per-queue prefix bytes for *tournament* queues (those
// with `prefix[1] == 1`) every time it composes the TYPE payload:
//
//   prefix[3] = tournament_get_races();
//   prefix[4] = tournament_is_arranged() ? tournament_get_game_type() : 0;
//
// This module reproduces that mutation as a pure function so it can
// be plugged into `compose_type_payload` without dragging in the
// legacy `tournament_*` globals. Hosts pass a `TournamentSnapshot`
// (already gathered from whatever source they like) and a base
// prefix table, and get back the decorated table.

#pragma once

#include <array>
#include <cstdint>

#include "application/anongame_inforeply/type_composer.hpp"

namespace pvpgn::application::anongame_inforeply {

/// View of the tournament state that affects TYPE-section prefixes.
/// Mirrors what the legacy TY block reads from `tournament_*` getters.
struct TournamentSnapshot {
    /// Bitmask of races available in the active tournament. Written
    /// into `prefix[3]` for every TY-flagged queue.
    std::uint8_t races = 0;

    /// Whether the active tournament is arranged-team. When false,
    /// `prefix[4]` is forced to 0; when true, `game_type` is written
    /// into `prefix[4]`.
    bool arranged = false;

    /// Arranged-team size (e.g. 2 for AT 2v2). Only used when
    /// `arranged == true`.
    std::uint8_t game_type = 0;

    bool operator==(const TournamentSnapshot&) const = default;
};

/// Apply the legacy tournament mutation to a base prefix table.
/// Only TY-flagged rows (`prefix[1] == 1`) are touched; PG/AT rows
/// pass through unchanged.
constexpr std::array<std::array<std::uint8_t, 5>, kAnonGameQueueCount>
decorate_prefix_for_tournament(
    const std::array<std::array<std::uint8_t, 5>, kAnonGameQueueCount>& base,
    const TournamentSnapshot& snap) {
    auto out = base;
    for (auto& row : out) {
        if (row[1] == 1) {  // TY-flagged queue
            row[3] = snap.races;
            row[4] = snap.arranged ? snap.game_type : std::uint8_t{0};
        }
    }
    return out;
}

}  // namespace pvpgn::application::anongame_inforeply
