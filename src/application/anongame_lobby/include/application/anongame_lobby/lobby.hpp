// SPDX-License-Identifier: GPL-2.0-or-later
//
// Interface-only header for the anonymous-game lobby state. The
// legacy code path (src/bnetd/anongame*) covers the entire
// match-making lifecycle: a client opts into FindAnonGame, gets
// queued by gametype + map + bracket, eventually two/four/eight
// candidates fill the bracket and the lobby promotes them to a
// game session.
//
// The dispatcher receives the current queue state + a new entrant
// and decides "enqueue", "drop", or "promote to game". The actual
// queue storage stays caller-side.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace pvpgn::application::anongame_lobby {

/// A single match-making slot snapshot.
struct LobbyEntry {
    std::uint32_t account_id  = 0;
    std::uint32_t client_tag  = 0;
    std::uint32_t game_type   = 0;
    std::uint16_t skill_level = 0;
};

/// Outcome of a lobby admit request.
enum class LobbyAdmitStatus : std::uint8_t {
    kQueued      = 0,  ///< Entry added to the queue; no game yet.
    kPromoted    = 1,  ///< Bracket filled; caller should start a game with `promoted_party`.
    kDuplicate   = 2,  ///< Account already in the queue for this gametype.
    kRejected    = 3,  ///< Match config disallows this combination.
};

struct LobbyAdmitRequest {
    LobbyEntry                     entrant;
    /// Caller-resolved view of the current queue for this gametype.
    std::span<const LobbyEntry>    current_queue;
    /// Required bracket size (2/4/8) -- caller resolves from
    /// `anongame_infos` config based on entrant.game_type.
    std::uint8_t                   bracket_size = 2;
};

struct LobbyAdmitResponse {
    LobbyAdmitStatus       status = LobbyAdmitStatus::kRejected;
    /// Populated on kPromoted -- the slots that should be removed
    /// from the queue and joined into a new game session.
    std::vector<LobbyEntry> promoted_party;
};

LobbyAdmitResponse dispatch_admit(LobbyAdmitRequest const& req);

}  // namespace pvpgn::application::anongame_lobby
