// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection_fsm.hpp
/// Higher-level domain FSM for a single Battle.net client connection.
///
/// ## Relationship to BnetFsm
///
/// `BnetFsm` (protocol/bnet/fsm.hpp) is a *wire-level* FSM: it validates
/// packet ordering and performs the immediate protocol replies (PING echo,
/// LOGON ack, ENTERCHAT echo).  It knows nothing about accounts, channels,
/// or games.
///
/// `ConnectionFsm` is a *domain-level* FSM that sits one layer above.  It:
///   - Owns the session lifecycle (Connecting → Authenticating → LoggedIn →
///     InChannel → InGame → Disconnecting)
///   - Dispatches raw BNCS packets (identified by SID byte) to the correct
///     handler for the current state
///   - Tracks per-connection domain state: account ID, channel ID, game ID
///   - Uses IConnectionContext for all I/O (send_packet / close)
///   - Does NOT own a BnetFsm — it replaces the wire-dance portion for the
///     states it implements; the remaining states still delegate to BnetFsm
///     via the strangler-fig bridge until fully migrated
///
/// ## State chart
///
///   ┌─────────────┐  SID_AUTH_INFO (0x50)   ┌───────────────────┐
///   │  Connecting │ ───────────────────────▶ │  Authenticating   │
///   └─────────────┘                          └────────┬──────────┘
///         │                                           │ SID_AUTH_ACCOUNTLOGON (0x53)
///         │ SID_LOGON_REQUEST (0x29)                  │ + SID_AUTH_ACCOUNTLOGONPROOF (0x54)
///         │ (legacy OLS auth path)                    ▼
///         │                                  ┌────────────────────┐
///         └────────────────────────────────▶ │     LoggedIn       │
///                                            └────────┬───────────┘
///                                                     │ SID_ENTERCHAT (0x0A)
///                                                     ▼
///                                            ┌────────────────────┐
///                                            │    InChannel       │◀─┐
///                                            └────────┬───────────┘  │
///                                                     │ SID_STARTADVEX* / SID_STARTADVEX3
///                                                     │ (StartGame)   │
///                                                     │ SID_GETADVLISTEX (JoinGame)
///                                                     ▼              │
///                                            ┌────────────────────┐  │
///                                            │     InGame         │──┘ SID_STOPADV (LeaveGame)
///                                            └────────────────────┘
///
/// `close()` from any state → Disconnecting (terminal).
/// Unknown packets in any non-Disconnecting state → silently ignored.
///
/// ## C++20 conventions
///   - No exceptions; errors returned as `core::Status<>`
///   - `std::optional<T>` for nullable values
///   - `std::span<const std::byte>` for packet payloads
///   - `[[nodiscard]]` on all status-returning methods

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "core/result.hpp"
#include "domain/connection/connection_context.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::connection {

// ---------------------------------------------------------------------------
// BNCS packet IDs handled by this FSM
// ---------------------------------------------------------------------------

/// Selected SID constants referenced by ConnectionFsm.
/// Full list lives in protocol/bnet/wire_types.hpp; these are the subset
/// that drive state transitions in the domain FSM.
namespace sid {
    inline constexpr std::uint8_t kAuthInfo              = 0x50;  ///< SID_AUTH_INFO
    inline constexpr std::uint8_t kAuthCheck             = 0x51;  ///< SID_AUTH_CHECK
    inline constexpr std::uint8_t kAuthAccountLogon      = 0x53;  ///< SID_AUTH_ACCOUNTLOGON
    inline constexpr std::uint8_t kAuthAccountLogonProof = 0x54;  ///< SID_AUTH_ACCOUNTLOGONPROOF
    inline constexpr std::uint8_t kLogonRequest          = 0x29;  ///< SID_LOGONRESPONSE (legacy OLS)
    inline constexpr std::uint8_t kLogonRequest2         = 0x3A;  ///< SID_LOGONRESPONSE2
    inline constexpr std::uint8_t kEnterChat             = 0x0A;  ///< SID_ENTERCHAT
    inline constexpr std::uint8_t kJoinChannel           = 0x0C;  ///< SID_JOINCHANNEL
    inline constexpr std::uint8_t kChatCommand           = 0x0E;  ///< SID_CHATCOMMAND
    inline constexpr std::uint8_t kLeaveChannel          = 0x28;  ///< SID_LEAVECHAT
    inline constexpr std::uint8_t kStartGame1            = 0x1C;  ///< SID_STARTADVEX
    inline constexpr std::uint8_t kStartGame3            = 0x1F;  ///< SID_STARTADVEX3
    inline constexpr std::uint8_t kJoinGame              = 0x09;  ///< SID_GETADVLISTEX (join)
    inline constexpr std::uint8_t kCloseGame             = 0x07;  ///< SID_STOPADV (leave/close game)
    inline constexpr std::uint8_t kPing                  = 0x25;  ///< SID_PING
    inline constexpr std::uint8_t kNull                  = 0x00;  ///< SID_NULL (keepalive)
}  // namespace sid

// ---------------------------------------------------------------------------
// ConnectionState enum
// ---------------------------------------------------------------------------

/// Domain-level connection lifecycle states.
///
/// Mapping from legacy `t_conn_state` (connection.h):
///
///   Legacy t_conn_state          │ v3 ConnectionState
///   ─────────────────────────────┼──────────────────────────────────────────
///   conn_state_empty             │ (pre-construction; not represented)
///   conn_state_initial           │ Connecting
///   conn_state_connected         │ Connecting  (same — waiting for AUTH_INFO)
///   conn_state_loggedin          │ LoggedIn / InChannel / InGame
///   conn_state_destroy           │ Disconnecting
///   conn_state_bot_username      │ Authenticating  (bot sub-state)
///   conn_state_bot_password      │ Authenticating  (bot sub-state)
///   conn_state_untrusted         │ Connecting  (pre-auth)
///   conn_state_pending_raw       │ Connecting  (raw/telnet init)
///
/// Mapping from legacy `t_conn_class`:
///
///   Legacy t_conn_class          │ v3 ConnectionState notes
///   ─────────────────────────────┼──────────────────────────────────────────
///   conn_class_init              │ Connecting (class not yet determined)
///   conn_class_bnet              │ All states (BNCS protocol)
///   conn_class_file              │ Connecting (BNFTP — separate FSM)
///   conn_class_bot               │ Authenticating (bot login sub-states)
///   conn_class_telnet            │ Connecting (telnet — separate FSM)
///   conn_class_ircinit           │ Connecting (IRC init)
///   conn_class_irc               │ LoggedIn / InChannel (IRC — separate FSM)
///   conn_class_wol               │ LoggedIn / InChannel (WOL — separate FSM)
///   conn_class_d2cs_bnetd        │ LoggedIn (D2CS link)
///   conn_class_w3route           │ InGame (W3 route)
///   conn_class_none              │ Disconnecting
enum class ConnectionState : std::uint8_t {
    Connecting,       ///< TCP accepted; waiting for SID_AUTH_INFO or SID_LOGON_REQUEST
    Authenticating,   ///< AUTH_INFO received; waiting for ACCOUNTLOGON + PROOF
    LoggedIn,         ///< Credentials verified; not yet in a channel
    InChannel,        ///< Joined a chat channel (ENTERCHAT + JOINCHANNEL done)
    InGame,           ///< Hosting or joined a game
    Disconnecting,    ///< Terminal state — close() called or fatal error
};

// ---------------------------------------------------------------------------
// ConnectionFsm
// ---------------------------------------------------------------------------

/// Domain-level connection state machine.
///
/// Constructed with an IConnectionContext reference (non-owning).
/// The context MUST outlive the FSM.
class ConnectionFsm {
public:
    /// Construct the FSM in the Connecting state.
    /// @param ctx        I/O context (send_packet / close). Non-owning ref.
    /// @param session_id Opaque session identity for logging / registry.
    explicit ConnectionFsm(IConnectionContext& ctx,
                           std::uint32_t session_id = 0) noexcept
        : ctx_(ctx), session_id_(session_id) {}

    // Non-copyable, non-movable (holds a reference to ctx_).
    ConnectionFsm(const ConnectionFsm&)            = delete;
    ConnectionFsm& operator=(const ConnectionFsm&) = delete;
    ConnectionFsm(ConnectionFsm&&)                 = delete;
    ConnectionFsm& operator=(ConnectionFsm&&)      = delete;

    ~ConnectionFsm() = default;

    // -----------------------------------------------------------------------
    // Observers
    // -----------------------------------------------------------------------

    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] std::uint32_t   session_id() const noexcept { return session_id_; }

    /// Account ID set after successful authentication (0 if not logged in).
    [[nodiscard]] std::uint32_t account_id() const noexcept { return account_id_; }

    /// Username as provided during login (empty if not logged in).
    [[nodiscard]] const std::string& username() const noexcept { return username_; }

    /// Game ID set when InGame (0 if not in a game).
    [[nodiscard]] std::uint32_t game_id() const noexcept { return game_id_; }

    // -----------------------------------------------------------------------
    // Primary dispatch entry point
    // -----------------------------------------------------------------------

    /// Dispatch a raw inbound BNCS packet to the appropriate handler.
    ///
    /// @param packet_id  The SID byte from the 4-byte BNCS header.
    /// @param payload    The packet body (bytes after the 4-byte header).
    ///
    /// @return ok()   — packet handled (or silently ignored as unknown).
    ///         fail() — fatal protocol error; caller should close session.
    [[nodiscard]] core::Status<> dispatch(std::uint8_t packet_id,
                                          std::span<const std::byte> payload);

    /// Initiate orderly shutdown from any state.
    /// Transitions to Disconnecting and calls ctx_.close().
    void close();

    // -----------------------------------------------------------------------
    // Per-state packet handlers (public for unit-test access)
    // -----------------------------------------------------------------------

    // --- Connecting state --------------------------------------------------

    /// Handle SID_AUTH_INFO (0x50): client product/version announcement.
    /// Valid in: Connecting
    /// Transition: Connecting → Authenticating
    [[nodiscard]] core::Status<> on_auth_info(std::span<const std::byte> payload);

    /// Handle SID_AUTH_CHECK (0x51): version check result from client.
    /// Valid in: Authenticating
    /// No state transition (stays Authenticating until ACCOUNTLOGON).
    [[nodiscard]] core::Status<> on_auth_check(std::span<const std::byte> payload);

    /// Handle SID_LOGON_REQUEST (0x29): legacy OLS single-step login.
    /// Valid in: Connecting
    /// Transition: Connecting → LoggedIn  (skips Authenticating)
    [[nodiscard]] core::Status<> on_logon_request(std::span<const std::byte> payload);

    // --- Authenticating state ----------------------------------------------

    /// Handle SID_AUTH_ACCOUNTLOGON (0x53): NLS SRP step 1.
    /// Valid in: Authenticating
    /// No state transition (waits for PROOF).
    [[nodiscard]] core::Status<> on_auth_accountlogon(std::span<const std::byte> payload);

    /// Handle SID_AUTH_ACCOUNTLOGONPROOF (0x54): NLS SRP step 2.
    /// Valid in: Authenticating
    /// Transition: Authenticating → LoggedIn  (on success)
    [[nodiscard]] core::Status<> on_auth_accountlogonproof(std::span<const std::byte> payload);

    // --- LoggedIn state ----------------------------------------------------

    /// Handle SID_ENTERCHAT (0x0A): client enters the chat environment.
    /// Valid in: LoggedIn
    /// Transition: LoggedIn → InChannel
    [[nodiscard]] core::Status<> on_enter_chat(std::span<const std::byte> payload);

    // --- InChannel state ---------------------------------------------------

    /// Handle SID_JOINCHANNEL (0x0C): client joins a named channel.
    /// Valid in: InChannel
    /// No state transition (stays InChannel).
    [[nodiscard]] core::Status<> on_join_channel(std::span<const std::byte> payload);

    /// Handle SID_CHATCOMMAND (0x0E): client sends a chat message or command.
    /// Valid in: InChannel
    /// No state transition.
    [[nodiscard]] core::Status<> on_chat_command(std::span<const std::byte> payload);

    /// Handle SID_LEAVECHAT (0x28): client leaves the chat environment.
    /// Valid in: InChannel
    /// Transition: InChannel → LoggedIn
    [[nodiscard]] core::Status<> on_leave_channel(std::span<const std::byte> payload);

    /// Handle SID_STARTADVEX (0x1C) / SID_STARTADVEX3 (0x1F): client creates a game.
    /// Valid in: InChannel
    /// Transition: InChannel → InGame
    [[nodiscard]] core::Status<> on_start_game(std::span<const std::byte> payload);

    /// Handle SID_GETADVLISTEX (0x09): client joins an existing game.
    /// Valid in: InChannel
    /// Transition: InChannel → InGame
    [[nodiscard]] core::Status<> on_join_game(std::span<const std::byte> payload);

    // --- InGame state ------------------------------------------------------

    /// Handle SID_STOPADV (0x07): client leaves/closes the current game.
    /// Valid in: InGame
    /// Transition: InGame → InChannel
    [[nodiscard]] core::Status<> on_leave_game(std::span<const std::byte> payload);

private:
    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /// Transition to Disconnecting and call ctx_.close().
    /// Returns a fail() status with the given reason for propagation.
    [[nodiscard]] core::Status<> reject(const char* reason);

    /// Build and send a minimal 4-byte BNCS reply with no body.
    [[nodiscard]] core::Status<> send_empty_reply(std::uint8_t packet_id);

    /// Build and send a BNCS reply with a 4-byte result code body.
    [[nodiscard]] core::Status<> send_result_reply(std::uint8_t packet_id,
                                                    std::uint32_t result);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    IConnectionContext& ctx_;
    std::uint32_t       session_id_{0};
    ConnectionState     state_{ConnectionState::Connecting};

    // Set after successful authentication
    std::uint32_t account_id_{0};
    std::string   username_;

    // Set while InGame (0 when not in a game)
    std::uint32_t game_id_{0};

    // Client product tag (4-byte big-endian, e.g. 'STAR', 'WAR3')
    // stored from SID_AUTH_INFO for downstream use
    std::uint32_t client_product_tag_{0};

    // Monotonically increasing game ID counter (per-FSM instance)
    std::uint32_t next_game_id_{1};
};

}  // namespace pvpgn::domain::connection
