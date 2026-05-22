// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file lua_connection_context.hpp
/// `IConnectionContext` implementation that fires Lua hooks on each domain
/// event, bridging the v3 pipeline into the existing `lua/` scripts.
///
/// ## Hook mapping
///
/// | IConnectionContext callback          | Lua function called                          |
/// |--------------------------------------|----------------------------------------------|
/// | `on_authenticated(username)`         | `handle_user_login(username)`                |
/// | `on_channel_joined(channel_name)`    | `handle_channel_userjoin(username, channel)` |
/// | `on_channel_left()`                  | `handle_channel_userleft(username, channel)` |
/// | `on_game_created(game_id, info)`     | `handle_game_create(username, name, type)`   |
/// | `on_game_joined(game_id, info)`      | `handle_game_userjoin(username, name)`       |
/// | `on_game_left(game_id)`              | `handle_game_userleft(username, name)`       |
/// | `on_disconnected()`                  | `handle_user_disconnect(username)`           |
///
/// Hook names are taken directly from the legacy `lua/handle_client.lua`,
/// `lua/handle_game.lua`, and `lua/handle_server.lua` scripts so that
/// existing Lua scripts work without modification.
///
/// ## Behaviour when Lua is unavailable
///
/// When `PVPGN_HAVE_LUA` is not defined (or the `LuaRuntime` was not
/// successfully opened), all domain callbacks are no-ops.  The I/O methods
/// (`send_packet`, `close`, `get_remote_address`, `get_session_id`) are
/// always forwarded to the wrapped inner context.
///
/// ## Lifetime
///
/// `LuaConnectionContext` holds a **non-owning reference** to a
/// `LuaRuntime`.  The `LuaRuntime` MUST outlive all `LuaConnectionContext`
/// instances that reference it (typically the runtime lives for the entire
/// server lifetime).
///
/// The wrapped inner `IConnectionContext` MUST also outlive this object.
///
/// ## C++20 conventions
///   - No exceptions; errors from Lua are logged to `std::cerr` and ignored
///   - `[[nodiscard]]` on all status-returning methods
///   - Non-copyable, non-movable (holds references)

#include <cstdint>
#include <span>
#include <string>

#include "core/result.hpp"
#include "domain/connection/connection_context.hpp"
#include "infra/lua/lua_runtime.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// LuaConnectionContext
// ---------------------------------------------------------------------------

/// `IConnectionContext` decorator that fires Lua hooks on domain events.
///
/// Wraps an underlying `IConnectionContext` for I/O operations and adds
/// Lua scripting callbacks for each domain lifecycle event.
class LuaConnectionContext final
    : public domain::connection::IConnectionContext {
public:
    /// Construct with an underlying I/O context and a shared Lua runtime.
    ///
    /// @param inner    The underlying context for send_packet / close /
    ///                 get_remote_address / get_session_id.  Non-owning ref;
    ///                 MUST outlive this object.
    /// @param runtime  The shared Lua VM.  Non-owning ref; MUST outlive this
    ///                 object.  May be an unopened runtime (is_open() == false)
    ///                 in which case all Lua hooks are silently skipped.
    LuaConnectionContext(domain::connection::IConnectionContext& inner,
                         infra::lua::LuaRuntime&                 runtime) noexcept;

    // Non-copyable, non-movable (holds references).
    LuaConnectionContext(const LuaConnectionContext&)            = delete;
    LuaConnectionContext& operator=(const LuaConnectionContext&) = delete;
    LuaConnectionContext(LuaConnectionContext&&)                 = delete;
    LuaConnectionContext& operator=(LuaConnectionContext&&)      = delete;

    ~LuaConnectionContext() override = default;

    // -----------------------------------------------------------------------
    // IConnectionContext — I/O forwarded to inner_
    // -----------------------------------------------------------------------

    [[nodiscard]] core::Status<> send_packet(
        std::uint8_t               packet_id,
        std::span<const std::byte> payload) override;

    void close() override;

    [[nodiscard]] std::string get_remote_address() const override;

    [[nodiscard]] std::uint32_t get_session_id() const override;

    // -----------------------------------------------------------------------
    // IConnectionContext — domain lifecycle callbacks (fire Lua hooks)
    // -----------------------------------------------------------------------

    /// Called when the player successfully authenticates.
    ///
    /// Stores `username` for use in subsequent Lua hook calls.
    /// Fires: `handle_user_login(username)`
    ///
    /// @note This method is an extension beyond the base `IConnectionContext`
    ///       interface.  It is called by `BnetConnectionAdapter` (or the
    ///       composition root) when authentication succeeds.
    void on_authenticated(std::string_view username);

    /// Called when the player joins a channel.
    ///
    /// Fires: `handle_channel_userjoin(username, channel_name)`
    void on_channel_joined(std::string_view channel_name);

    /// Called when the player leaves a channel.
    ///
    /// Fires: `handle_channel_userleft(username, channel_name)`
    /// Uses the last channel name stored by `on_channel_joined`.
    void on_channel_left();

    /// Called when the player successfully creates/starts a new game.
    ///
    /// Fires: `handle_game_create(username, game_name, game_type_str)`
    void on_game_created(std::uint32_t                          game_id,
                         const domain::connection::GameInfo&    info) override;

    /// Called when the player successfully joins an existing game.
    ///
    /// Fires: `handle_game_userjoin(username, game_name)`
    void on_game_joined(std::uint32_t                          game_id,
                        const domain::connection::GameInfo&    info) override;

    /// Called when the player leaves a game.
    ///
    /// Fires: `handle_game_userleft(username, game_name)`
    /// Uses the last game name stored by `on_game_created` / `on_game_joined`.
    void on_game_left(std::uint32_t game_id) override;

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Returns the username set by `on_authenticated()`.
    /// Empty string if not yet authenticated.
    [[nodiscard]] const std::string& username() const noexcept {
        return username_;
    }

private:
    domain::connection::IConnectionContext& inner_;
    infra::lua::LuaRuntime&                 runtime_;

    std::string username_;       ///< Set by on_authenticated()
    std::string channel_name_;   ///< Set by on_channel_joined()
    std::string game_name_;      ///< Set by on_game_created() / on_game_joined()

    /// Convert a `GameType` enum to a short string for Lua scripts.
    static std::string game_type_str(domain::connection::GameType t);
};

}  // namespace pvpgn::app::bnetd
