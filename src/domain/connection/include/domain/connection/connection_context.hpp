// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection_context.hpp
/// Abstract I/O interface injected into ConnectionFsm.
///
/// Concrete implementations:
///   - Production: wraps a v3 TcpSession (Boost.Asio)
///   - Test: FakeConnectionContext (captures sent packets)
///   - Legacy bridge: wraps a t_connection* via conn_push_outqueue
///
/// The FSM is pure — no I/O, no clock, no logging.
/// All side-effects go through this interface.

#include <cstdint>
#include <span>
#include <string>

#include "core/result.hpp"

namespace pvpgn::domain::connection {

// ---------------------------------------------------------------------------
// GameInfo — metadata carried by game lifecycle events
// ---------------------------------------------------------------------------

/// Game type tag (maps to legacy `t_game_type` / `t_clienttag` game modes).
enum class GameType : std::uint8_t {
    Melee       = 0,  ///< Standard melee / ladder game
    FreeForAll  = 1,  ///< Free-for-all
    OneOnOne    = 2,  ///< 1v1 / duel
    Cooperative = 3,  ///< Co-op / team game
    Custom      = 4,  ///< Custom / scenario
};

/// Metadata attached to StartGame / JoinGame events.
///
/// All string fields are UTF-8.  `password` is empty for public games.
/// `max_players` of 0 means "use game-type default".
struct GameInfo {
    std::string   game_name;    ///< Human-readable game name (lobby title)
    std::string   game_stats;   ///< Encoded stats/map string (legacy statstring)
    std::string   password;     ///< Game password (empty = public)
    GameType      game_type{GameType::Melee};
    std::uint8_t  max_players{0};  ///< 0 = use default for game type
};

// ---------------------------------------------------------------------------
// IConnectionContext
// ---------------------------------------------------------------------------

/// Abstract I/O context for ConnectionFsm.
///
/// Lifetime: the context MUST outlive the FSM that holds a reference to it.
class IConnectionContext {
public:
    virtual ~IConnectionContext() = default;

    /// Send a raw BNCS packet (header already included) to the remote peer.
    /// @param packet_id  The SID byte (e.g. 0x50 for SID_AUTH_INFO).
    /// @param payload    The packet body bytes (after the 4-byte header).
    /// @return           ok() on success; fail() if the session is closing.
    [[nodiscard]] virtual core::Status<> send_packet(
        std::uint8_t packet_id,
        std::span<const std::byte> payload) = 0;

    /// Request orderly session shutdown.
    /// After this call the FSM transitions to Disconnecting.
    virtual void close() = 0;

    /// Remote peer address as a dotted-decimal string (e.g. "192.168.1.1").
    [[nodiscard]] virtual std::string get_remote_address() const = 0;

    /// Opaque session identifier (monotonically increasing per process).
    [[nodiscard]] virtual std::uint32_t get_session_id() const = 0;

    // -----------------------------------------------------------------------
    // Game lifecycle callbacks
    // -----------------------------------------------------------------------

    /// Called when the player successfully creates/starts a new game.
    /// @param game_id   Opaque game identifier assigned by the domain layer.
    /// @param info      Metadata for the newly created game.
    virtual void on_game_created(std::uint32_t game_id,
                                 const GameInfo& info) = 0;

    /// Called when the player successfully joins an existing game.
    /// @param game_id   Opaque game identifier of the joined game.
    /// @param info      Metadata for the joined game.
    virtual void on_game_joined(std::uint32_t game_id,
                                const GameInfo& info) = 0;

    /// Called when the player leaves a game and returns to the channel.
    /// @param game_id   Opaque game identifier of the game that was left.
    virtual void on_game_left(std::uint32_t game_id) = 0;
};

}  // namespace pvpgn::domain::connection
