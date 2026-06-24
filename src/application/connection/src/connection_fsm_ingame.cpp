// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_ingame.cpp
/// ConnectionFsm — InGame-state handlers and cross-state special handlers.
///
/// Handlers in this TU:
///   on_leave_game()       — SID_STOPADV (0x07): leave/close the current game
///   on_d2_char_select()   — SID_D2GAMELISTEX (0x68): D2 character select
///   on_warcraft_general() — SID_WARCRAFTGENERAL (0x44): WAR3 route token

#include "application/connection/connection_fsm.hpp"

#include <span>

#include "core/error.hpp"

#include "connection_fsm_internal.hpp"

namespace pvpgn::application::connection {

using namespace detail;

// ---------------------------------------------------------------------------
// InGame state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_leave_game(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InGame) {
        // Silently ignore if not in a game (some clients send STOPADV spuriously)
        return core::ok();
    }

    (void)payload;

    const std::uint32_t left_game_id = game_id_;
    game_id_ = 0u;
    state_   = ConnectionState::InChannel;

    // Notify the context
    ctx_.on_game_left(left_game_id);

    return core::ok();
}

// ---------------------------------------------------------------------------
// D2 character select handler
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_d2_char_select(
    std::span<const std::byte> payload) {
    // SID_D2GAMELISTEX (0x68) body layout (D2 character-select variant):
    //   [0]      char_class  (uint8)
    //   [1]      char_level  (uint8)
    //   [2..]    char_name   (NUL-terminated)
    //
    // This packet is legal in LoggedIn, InChannel, and InGame states.
    // Silently ignore in Connecting / Authenticating / Disconnecting.
    if (state_ == ConnectionState::Connecting    ||
        state_ == ConnectionState::Authenticating ||
        state_ == ConnectionState::Disconnecting) {
        return core::ok();
    }

    if (payload.size() < 3) {
        // Too short to contain class + level + at least one name byte.
        return core::ok();
    }

    const std::uint8_t char_class = static_cast<std::uint8_t>(payload[0]);
    const std::uint8_t char_level = static_cast<std::uint8_t>(payload[1]);
    const std::string  char_name  = read_cstring(payload, 2);

    bind_d2_character(char_name, char_class, char_level);
    return core::ok();
}

// ---------------------------------------------------------------------------
// WAR3 route token handler
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_warcraft_general(
    std::span<const std::byte> payload) {
    // SID_WARCRAFTGENERAL (0x44) body layout:
    //   [0]      subcommand  (uint8)
    //   [1..4]   route_token (LE uint32) — present in WID_GAMESEARCH (0x00)
    //            and several other subcommands.
    //
    // We extract the token from any subcommand that carries it at [1..4].
    // The token is used by RouteRegistry to pair the primary and route
    // connections.  Silently ignore if the payload is too short.
    if (payload.size() < 5) {
        return core::ok();
    }

    // The route token is at bytes [1..4] (LE uint32) for the subcommands
    // that carry it (WID_GAMESEARCH = 0x00, WID_CANCELSEARCH = 0x01, etc.).
    const std::uint32_t token = read_le32(payload, 1);
    set_war3_route_token(token);
    return core::ok();
}

}  // namespace pvpgn::application::connection
