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
///
/// ## LoginUser auth wiring
///
/// When constructed with a `LoginUser&` reference, `on_pass()` calls
/// `login_user_.execute(LoginRequest{...})` using OLS (old-style) auth.
/// On success the FSM transitions to Authenticated and sends 001 RPL_WELCOME.
/// On failure it sends 464 :Password incorrect and closes the connection.
///
/// When constructed without a `LoginUser` (legacy skeleton mode) the FSM
/// accepts any non-empty nick+user combination as before.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "application/auth/login_user.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/wol/wol_session_context.hpp"

// Forward-declare use-case types to avoid pulling in all their headers into
// every translation unit that includes wol_fsm.hpp.
namespace pvpgn::application::auth {
class LoginUser;
}  // namespace pvpgn::application::auth

namespace pvpgn::application::chat {
class JoinChannel;
class ListChannels;
class PostMessage;
}  // namespace pvpgn::application::chat

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
    /// Construct without auth use-case (skeleton / test mode).
    /// on_pass() accepts any non-empty nick+user combination.
    explicit WolFsm(std::shared_ptr<IWolSessionContext> ctx) noexcept
        : ctx_(std::move(ctx)), login_user_(nullptr) {}

    /// Construct with a LoginUser use-case (production mode).
    /// on_pass() calls login_user_.execute() for OLS credential check.
    /// @param ctx        Session I/O context. Non-owning shared ownership.
    /// @param login_user OLS authentication use-case. Non-owning ref;
    ///                   must outlive the FSM.
    WolFsm(std::shared_ptr<IWolSessionContext> ctx,
           application::auth::LoginUser& login_user) noexcept
        : ctx_(std::move(ctx)), login_user_(&login_user) {}

    /// Construct with full chat use-cases (production mode).
    /// @param ctx          Session I/O context.
    /// @param login_user   OLS authentication use-case (non-owning, may be null).
    /// @param list_channels ListChannels use-case for LIST command.
    /// @param join_channel  JoinChannel use-case for JOIN command.
    /// @param post_message  PostMessage use-case for PRIVMSG command.
    WolFsm(std::shared_ptr<IWolSessionContext> ctx,
           application::auth::LoginUser* login_user,
           std::shared_ptr<application::chat::ListChannels> list_channels,
           std::shared_ptr<application::chat::JoinChannel>  join_channel,
           std::shared_ptr<application::chat::PostMessage>  post_message) noexcept
        : ctx_(std::move(ctx))
        , login_user_(login_user)
        , list_channels_(std::move(list_channels))
        , join_channel_(std::move(join_channel))
        , post_message_(std::move(post_message)) {}

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

    /// Attempt authentication with the current nick_/user_/pass_ credentials.
    /// Called from on_pass() and on_user() once all three are available.
    /// @param close_on_bad_credentials  When true, close the connection on any
    ///   auth failure (used when PASS arrived before NICK/USER — the client
    ///   cannot retry in that ordering).  When false, only close on
    ///   UnknownUser; InvalidCredentials keeps the connection open for retry.
    /// Returns ok() in all cases (auth failure is communicated via IRC numerics).
    core::Status<> try_authenticate(bool close_on_bad_credentials);

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

    /// Non-owning pointer to the OLS auth use-case.
    /// Null when constructed without auth (skeleton / test mode).
    application::auth::LoginUser* login_user_;

    /// Chat use-cases. Null when not wired (stub mode).
    std::shared_ptr<application::chat::ListChannels> list_channels_;
    std::shared_ptr<application::chat::JoinChannel>  join_channel_;
    std::shared_ptr<application::chat::PostMessage>  post_message_;

    WolState    state_   = WolState::Connecting;
    std::string nick_;
    std::string user_;
    std::string realname_;
    std::string pass_;
    std::string channel_;

    /// Account ID resolved after successful login (0 until authenticated).
    domain::AccountId account_id_{0};

    /// Channel ID of the currently joined channel (0 until JOIN succeeds).
    domain::ChannelId channel_id_{0};

    /// Session ID for this connection (used in use-case calls).
    domain::SessionId session_id_{};

    /// Accumulation buffer for partial lines.
    std::string line_buf_;
};

}  // namespace pvpgn::protocol::wol
