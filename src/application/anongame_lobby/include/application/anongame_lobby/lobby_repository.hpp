// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file lobby_repository.hpp
/// Port interface for the anongame match-making queue store.
///
/// The dispatcher in `lobby.hpp` is stateless: it operates on a
/// caller-resolved `current_queue` span and a `bracket_size`. The
/// repository hides where those values come from -- in the legacy
/// path it's the `anongame_queue` global walked by client_tag +
/// game_type; in tests it's an in-memory `std::vector`.

#include "application/anongame_lobby/lobby.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace pvpgn::application::anongame_lobby {

/// Port: queue storage for the anongame lobby.
class IAnonGameLobbyRepository {
public:
    virtual ~IAnonGameLobbyRepository() = default;

    /// Snapshot of the current waiting party for the given
    /// gametype. Returned by value so the caller can pass it
    /// straight into `LobbyAdmitRequest.current_queue` without
    /// worrying about concurrent mutation.
    virtual std::vector<LobbyEntry> queue_for(std::uint32_t game_type) const = 0;

    /// Bracket size (2/4/8/...) required to promote a queue full of
    /// entrants to a started game. Resolved from `anongame_infos`
    /// config in the legacy adapter.
    virtual std::uint8_t bracket_size_for(std::uint32_t game_type) const = 0;

    /// Append `entry` to the queue for `entry.game_type`. Called
    /// after the dispatcher returns `kQueued`.
    virtual void add(LobbyEntry const& entry) = 0;

    /// Remove the given party from the queue. Called after the
    /// dispatcher returns `kPromoted` and the caller has started a
    /// new game session.
    virtual void remove_party(std::span<const LobbyEntry> party) = 0;
};

}  // namespace pvpgn::application::anongame_lobby
