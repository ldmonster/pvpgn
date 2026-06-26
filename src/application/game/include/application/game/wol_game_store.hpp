// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_game_store.hpp
/// Port for the Westwood Online (WOL) game-channel registry.
///
/// In WOL a "game" IS a channel carrying extra game metadata: min/max players,
/// the channel/game type tag, a tournament flag, an opaque game-extension blob,
/// and an optional password. A client CREATEs a game with `JOINGAME #name <min>
/// <max> <type> ...` and others JOIN it with `JOINGAME #name ...`; the server
/// tracks the game so `JOINGAME` (join mode) can find it, check full/password,
/// and reproduce the WOLv1/WOLv2 JOINGAME acknowledgement. See `pvpgn-server`
/// `handle_wol.cpp` `_handle_joingame_command` + `gamelist_find_game_available`.
///
/// Kept separate from the BNCS-oriented IGameRepository (which models a hosted
/// match advertised via SID_STARTADVEX3/GETADVLISTEX with different fields) so
/// neither model is burdened with the other's protocol specifics — exactly like
/// [[wol_credential_store]] is kept apart from IAccountRepository.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/shared/ids.hpp"

namespace pvpgn::application::game {

/// A WOL game-channel and its current occupancy.
struct WolGameInfo {
    std::string       name;                ///< game/channel name (no '#')
    std::uint32_t     min_players   = 0;   ///< minimum players (channel_get_min)
    std::uint32_t     max_players   = 0;   ///< maximum players (game_get_maxplayers)
    std::uint32_t     game_type     = 0;   ///< WOL channel/game type tag
    std::uint32_t     tournament    = 0;   ///< 1 = ladder/tournament, else 0
    std::string       game_extension = "0"; ///< opaque WOLv2 game-extension blob
    std::string       password;            ///< "" = open game
    domain::AccountId host{0};             ///< creator account
    domain::ChannelId channel_id{0};       ///< backing chat channel
    std::vector<domain::AccountId> players;///< current players (incl. host)

    [[nodiscard]] bool is_full() const noexcept {
        return max_players != 0 && players.size() >= max_players;
    }
};

/// Port: registry of open WOL game-channels, keyed by game name.
class IWolGameStore {
public:
    virtual ~IWolGameStore() = default;

    /// Register a newly created game (replacing any prior game of the same name).
    virtual void create(const WolGameInfo& game) = 0;

    /// Look up a game by name (case-insensitive). nullopt if none is open.
    [[nodiscard]] virtual std::optional<WolGameInfo>
    find(std::string_view name) const = 0;

    /// Atomically add @p player to the named game. Returns false if the game is
    /// unknown or already full; true (and records the player) otherwise. A
    /// player already in the game counts as success without duplication.
    virtual bool add_player(std::string_view name, domain::AccountId player) = 0;

    /// Remove the named game (e.g. when it closes). No-op if unknown.
    virtual void remove(std::string_view name) = 0;

    /// Remove @p player from the named game (e.g. on disconnect). If the game
    /// becomes empty it is erased. No-op if the game/player is unknown.
    virtual void remove_player(std::string_view name, domain::AccountId player) = 0;
};

}  // namespace pvpgn::application::game
