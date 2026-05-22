// SPDX-License-Identifier: GPL-2.0-or-later

/// @file lua_connection_context.cpp
/// Implementation of `LuaConnectionContext`.
///
/// Hook name mapping (matches legacy lua/ scripts):
///
///   on_authenticated(username)          → handle_user_login(username)
///   on_channel_joined(channel_name)     → handle_channel_userjoin(username, channel)
///   on_channel_left()                   → handle_channel_userleft(username, channel)
///   on_game_created(game_id, info)      → handle_game_create(username, game_name, game_type)
///   on_game_joined(game_id, info)       → handle_game_userjoin(username, game_name)
///   on_game_left(game_id)               → handle_game_userleft(username, game_name)
///   on_disconnected()                   → handle_user_disconnect(username)
///
/// All hook calls silently skip if the Lua function does not exist.
/// Lua errors are printed to std::cerr and ignored (no-exception policy).

#include "app/bnetd/lua_connection_context.hpp"

#include <iostream>

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LuaConnectionContext::LuaConnectionContext(
    domain::connection::IConnectionContext& inner,
    infra::lua::LuaRuntime&                 runtime) noexcept
    : inner_(inner)
    , runtime_(runtime) {}

// ---------------------------------------------------------------------------
// IConnectionContext — I/O forwarded to inner_
// ---------------------------------------------------------------------------

core::Status<> LuaConnectionContext::send_packet(
    std::uint8_t               packet_id,
    std::span<const std::byte> payload) {
    return inner_.send_packet(packet_id, payload);
}

void LuaConnectionContext::close() {
    inner_.close();
}

std::string LuaConnectionContext::get_remote_address() const {
    return inner_.get_remote_address();
}

std::uint32_t LuaConnectionContext::get_session_id() const {
    return inner_.get_session_id();
}

// ---------------------------------------------------------------------------
// Domain lifecycle callbacks — fire Lua hooks
// ---------------------------------------------------------------------------

void LuaConnectionContext::on_authenticated(std::string_view username) {
    username_ = std::string{username};

    // Lua hook: handle_user_login(username)
    if (auto err = runtime_.call_hook("handle_user_login", username_)) {
        std::cerr << "[LuaConnectionContext] handle_user_login error: "
                  << *err << "\n";
    }
}

void LuaConnectionContext::on_channel_joined(std::string_view channel_name) {
    channel_name_ = std::string{channel_name};

    // Lua hook: handle_channel_userjoin(username, channel_name)
    if (auto err = runtime_.call_hook("handle_channel_userjoin",
                                      username_, channel_name_)) {
        std::cerr << "[LuaConnectionContext] handle_channel_userjoin error: "
                  << *err << "\n";
    }
}

void LuaConnectionContext::on_channel_left() {
    // Lua hook: handle_channel_userleft(username, channel_name)
    if (auto err = runtime_.call_hook("handle_channel_userleft",
                                      username_, channel_name_)) {
        std::cerr << "[LuaConnectionContext] handle_channel_userleft error: "
                  << *err << "\n";
    }
    channel_name_.clear();
}

void LuaConnectionContext::on_game_created(
    std::uint32_t                       /*game_id*/,
    const domain::connection::GameInfo& info) {

    game_name_ = info.game_name;

    // Lua hook: handle_game_create(username, game_name, game_type)
    const std::string type_str = game_type_str(info.game_type);
    if (auto err = runtime_.call_hook("handle_game_create",
                                      username_, game_name_, type_str)) {
        std::cerr << "[LuaConnectionContext] handle_game_create error: "
                  << *err << "\n";
    }
}

void LuaConnectionContext::on_game_joined(
    std::uint32_t                       /*game_id*/,
    const domain::connection::GameInfo& info) {

    game_name_ = info.game_name;

    // Lua hook: handle_game_userjoin(username, game_name)
    if (auto err = runtime_.call_hook("handle_game_userjoin",
                                      username_, game_name_)) {
        std::cerr << "[LuaConnectionContext] handle_game_userjoin error: "
                  << *err << "\n";
    }
}

void LuaConnectionContext::on_game_left(std::uint32_t /*game_id*/) {
    // Lua hook: handle_game_userleft(username, game_name)
    if (auto err = runtime_.call_hook("handle_game_userleft",
                                      username_, game_name_)) {
        std::cerr << "[LuaConnectionContext] handle_game_userleft error: "
                  << *err << "\n";
    }
    game_name_.clear();
}

// ---------------------------------------------------------------------------
// game_type_str — helper
// ---------------------------------------------------------------------------

std::string LuaConnectionContext::game_type_str(
    domain::connection::GameType t) {
    using domain::connection::GameType;
    switch (t) {
        case GameType::Melee:       return "melee";
        case GameType::FreeForAll:  return "ffa";
        case GameType::OneOnOne:    return "1v1";
        case GameType::Cooperative: return "coop";
        case GameType::Custom:      return "custom";
    }
    return "unknown";
}

}  // namespace pvpgn::app::bnetd
