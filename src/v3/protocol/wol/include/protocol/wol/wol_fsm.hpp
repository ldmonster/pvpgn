// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_fsm.hpp
/// WOL (Westwood Online) chat protocol FSM.
///
/// WOL is an IRC-like text protocol used by Command & Conquer, Red Alert,
/// Tiberian Sun, and other Westwood/EA games. It is line-oriented (\r\n
/// terminated) and uses IRC-style commands with WOL-specific extensions.
///
/// Authentication flow:
///   Client → NICK <nickname>
///   Client → USER <username> <hostname> <servername> :<realname>
///   Client → PASS <password>
///   Server → :server 001 <nick> :Welcome to WOL
///
/// State chart:
///
///   ┌────────────┐  NICK   ┌───────────────┐  USER+PASS  ┌─────────────┐
///   │ Connecting │────────▶│ Authenticating │────────────▶│Authenticated│
///   └────────────┘         └───────────────┘             └──────┬──────┘
///                                                               │ JOIN
///                                                        ┌──────▼──────┐
///                                                        │  InChannel  │
///                                                        └─────────────┘
///
/// All states accept PING (→ PONG) and QUIT (→ Disconnecting).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "protocol/wol/wol_session_context.hpp"

namespace pvpgn::protocol::wol {

/// WOL chat protocol FSM states.
enum class WolState : std::uint8_t {
    Connecting,      ///< Initial state — waiting for NICK
    Authenticating,  ///< Received NICK — waiting for USER / PASS
    Authenticated,   ///< Logged in — normal operation
    InChannel,       ///< Joined a channel
    InGame,          ///< In a game lobby
    Disconnecting,   ///< Graceful disconnect in progress
};

/// WOL chat protocol FSM.
///
/// Accumulates raw bytes into a line buffer, splits on \r\n, and
/// dispatches each complete line to the appropriate command handler.
class WolFsm {
public:
    explicit WolFsm(std::shared_ptr<IWolSessionContext> ctx) noexcept
        : ctx_(std::move(ctx)) {}

    /// Feed raw bytes from the TCP stream into the FSM.
    /// Returns ok() on success; error causes the session to close.
    core::Status<> on_bytes(std::span<const std::byte> bytes);

    /// Called when the connection is closing (peer close or error).
    void on_close();

    /// Current FSM state (for testing / introspection).
    WolState state() const noexcept { return state_; }

    /// Current nickname (empty until NICK received).
    std::string_view nick() const noexcept { return nick_; }

    /// Current channel (empty until JOIN).
    std::string_view channel() const noexcept { return channel_; }

private:
    // -----------------------------------------------------------------------
    // Line processing
    // -----------------------------------------------------------------------

    /// Process all complete \r\n-terminated lines in line_buf_.
    core::Status<> process_lines();

    /// Dispatch one complete line (without the trailing \r\n).
    core::Status<> dispatch_line(std::string_view line);

    // -----------------------------------------------------------------------
    // Command handlers
    // -----------------------------------------------------------------------

    /// NICK <nickname>
    core::Status<> on_nick(std::string_view params);

    /// USER <username> <hostname> <servername> :<realname>
    core::Status<> on_user(std::string_view params);

    /// PASS <password>
    core::Status<> on_pass(std::string_view params);

    /// PING <token>
    core::Status<> on_ping(std::string_view params);

    /// QUIT [:<reason>]
    core::Status<> on_quit(std::string_view params);

    /// LIST
    core::Status<> on_list(std::string_view params);

    /// JOIN #<channel>
    core::Status<> on_join(std::string_view params);

    /// PART #<channel>
    core::Status<> on_part(std::string_view params);

    /// PRIVMSG <target> :<message>
    core::Status<> on_privmsg(std::string_view params);

    // -----------------------------------------------------------------------
    // Reply helpers
    // -----------------------------------------------------------------------

    /// Send ":server_name <code> <target> :<text>\r\n"
    core::Status<> send_numeric(int code,
                                std::string_view target,
                                std::string_view text);

    /// Send a raw line (appends \r\n).
    core::Status<> send_raw(std::string_view line);

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    std::shared_ptr<IWolSessionContext> ctx_;

    WolState    state_   = WolState::Connecting;
    std::string nick_;
    std::string user_;
    std::string realname_;
    std::string pass_;
    std::string channel_;

    /// Accumulation buffer for partial lines.
    std::string line_buf_;
};

}  // namespace pvpgn::protocol::wol
