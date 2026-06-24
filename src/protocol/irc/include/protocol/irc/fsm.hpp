// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fsm.hpp
/// Minimal RFC 1459 server-side session FSM.
///
/// State chart:
///
///     ┌───────────┐  NICK+USER   ┌─────────────┐  JOIN  ┌───────────┐
///     │ Greeting  │─────────────▶│ Registered  │───────▶│ InChannel │
///     └───────────┘              └─────────────┘        └───────────┘
///
/// Commands handled by the FSM:
///   * NICK / USER  — registration handshake; emits 001 RPL_WELCOME.
///   * PING         — answered immediately with PONG.
///   * PONG         — client keepalive reply; accepted and ignored (no-op).
///   * JOIN         — transitions to InChannel; calls JoinChannel use-case
///                    when wired; echoes JOIN + 332 RPL_TOPIC + 353 RPL_NAMREPLY
///                    + 366 RPL_ENDOFNAMES.
///   * PART         — leaves channel; calls LeaveChannel use-case when wired;
///                    echoes PART; transitions back to Registered.
///   * PRIVMSG      — channel messages call PostMessage use-case when wired;
///                    private messages return 401 ERR_NOSUCHNICK.
///   * NOTICE       — echoes back.
///   * QUIT         — closes session.
///   * AWAY         — sets/clears away status; replies 305/306.
///   * WHOIS <nick> — returns 311 RPL_WHOISUSER + 318 RPL_ENDOFWHOIS (self only).
///   * WHO  <mask>  — returns 352 RPL_WHOREPLY + 315 RPL_ENDOFWHO.
///   * MODE         — returns 324 RPL_CHANNELMODEIS (no-op skeleton).
///   * TOPIC        — returns 332 RPL_TOPIC or 331 RPL_NOTOPIC.
///   * NAMES        — returns 353 RPL_NAMREPLY + 366 RPL_ENDOFNAMES.
///   * KICK         — returns 482 ERR_CHANOPRIVSNEEDED.
///   * MOTD         — returns 375 RPL_MOTDSTART + 376 RPL_ENDOFMOTD.
///   * LIST         — calls ListChannels use-case when wired; returns
///                    321 RPL_LISTSTART + 322 RPL_LIST + 323 RPL_LISTEND.
///
/// Error replies:
///   * 401 ERR_NOSUCHNICK   — WHOIS/WHO for unknown nick; PRIVMSG to nick.
///   * 403 ERR_NOSUCHCHANNEL — channel commands with no channel.
///   * 404 ERR_CANNOTSENDTOCHAN — PostMessage failure.
///   * 411 ERR_NORECIPIENT  — PRIVMSG/NOTICE with missing params.
///   * 421 ERR_UNKNOWNCOMMAND — unrecognised command.
///   * 431 ERR_NONICKNAMEGIVEN — NICK with empty param.
///   * 451 ERR_NOTREGISTERED — commands requiring registration.
///   * 461 ERR_NEEDMOREPARAMS — commands with too few params.
///   * 474 ERR_BANNEDFROMCHAN — JOIN when banned.
///   * 482 ERR_CHANOPRIVSNEEDED — KICK (stub).
///
/// Chat use-case wiring:
///   When constructed with use-case pointers, the FSM calls the real
///   domain use-cases for JOIN, PART, PRIVMSG, and LIST.
///   When use-cases are null (skeleton / test mode), the FSM falls back
///   to the local-state skeleton behaviour.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "application/auth/login_user.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/irc/message.hpp"
#include "protocol/irc/session_context.hpp"

// Forward-declare use-case types to avoid pulling in all their headers.
namespace pvpgn::application::auth {
class LoginUser;
}  // namespace pvpgn::application::auth

namespace pvpgn::application::chat {
class JoinChannel;
class ListChannels;
class PostMessage;
class LeaveChannel;
}  // namespace pvpgn::application::chat

namespace pvpgn::protocol::irc {

enum class IrcState : std::uint8_t {
    Greeting,    ///< need NICK + USER (and optionally PASS)
    Registered,  ///< handshake done
    InChannel,   ///< joined exactly one channel (skeleton limit)
    Closing,
};

class IrcFsm {
public:
    /// Construct without auth use-case (skeleton / test mode).
    /// Registration completes on NICK + USER without credential check.
    explicit IrcFsm(ISessionContext& ctx) noexcept
        : ctx_(&ctx), login_user_(nullptr) {}

    /// Construct with a LoginUser use-case (production mode).
    /// Registration requires NICK + USER + PASS; on_pass() calls
    /// login_user_.execute() for credential verification.
    /// @param ctx        Session I/O context. Non-owning ref.
    /// @param login_user OLS authentication use-case. Non-owning ref;
    ///                   must outlive the FSM.
    IrcFsm(ISessionContext& ctx,
           application::auth::LoginUser& login_user) noexcept
        : ctx_(&ctx), login_user_(&login_user) {}

    /// Construct with full chat use-cases (production mode).
    /// @param ctx           Session I/O context. Non-owning ref.
    /// @param login_user    OLS authentication use-case (non-owning, may be null).
    /// @param list_channels ListChannels use-case for LIST command.
    /// @param join_channel  JoinChannel use-case for JOIN command.
    /// @param post_message  PostMessage use-case for PRIVMSG command.
    /// @param leave_channel LeaveChannel use-case for PART command.
    IrcFsm(ISessionContext& ctx,
           application::auth::LoginUser* login_user,
           std::shared_ptr<application::chat::ListChannels> list_channels,
           std::shared_ptr<application::chat::JoinChannel>  join_channel,
           std::shared_ptr<application::chat::PostMessage>  post_message,
           std::shared_ptr<application::chat::LeaveChannel> leave_channel) noexcept
        : ctx_(&ctx)
        , login_user_(login_user)
        , list_channels_(std::move(list_channels))
        , join_channel_(std::move(join_channel))
        , post_message_(std::move(post_message))
        , leave_channel_(std::move(leave_channel)) {}

    [[nodiscard]] IrcState         state()      const noexcept { return state_; }
    [[nodiscard]] std::string_view nick()       const noexcept { return nick_; }
    [[nodiscard]] std::string_view user()       const noexcept { return user_; }
    [[nodiscard]] std::string_view channel()    const noexcept { return channel_; }
    [[nodiscard]] std::string_view away_msg()   const noexcept { return away_msg_; }
    [[nodiscard]] bool             is_away()    const noexcept { return !away_msg_.empty(); }
    [[nodiscard]] domain::AccountId account_id() const noexcept { return account_id_; }
    [[nodiscard]] domain::ChannelId channel_id() const noexcept { return channel_id_; }

    /// Drive the FSM with one decoded inbound message.
    core::Status<> handle(const Message& msg);

private:
    // ---- registration -------------------------------------------------------
    core::Status<> on_nick(const Message&);
    core::Status<> on_user(const Message&);
    /// PASS <password> — store pending_password_ for use in
    /// try_complete_registration(). Silently ignored after registration.
    core::Status<> on_pass(const Message&);
    core::Status<> try_complete_registration();

    // ---- always-available ---------------------------------------------------
    core::Status<> on_ping(const Message&);
    /// PONG — client keepalive reply. Accepted and ignored (no-op success),
    /// matching the original's tolerant _handle_pong_command. Without this a
    /// client's PONG would wrongly fall through to 421 ERR_UNKNOWNCOMMAND.
    core::Status<> on_pong(const Message&);
    core::Status<> on_quit(const Message&);
    core::Status<> on_motd(const Message&);

    // ---- post-registration --------------------------------------------------
    core::Status<> on_join(const Message&);
    core::Status<> on_part(const Message&);
    core::Status<> on_privmsg(const Message&);
    core::Status<> on_notice(const Message&);
    core::Status<> on_away(const Message&);
    core::Status<> on_whois(const Message&);
    core::Status<> on_who(const Message&);
    core::Status<> on_mode(const Message&);
    core::Status<> on_topic(const Message&);
    core::Status<> on_names(const Message&);
    core::Status<> on_kick(const Message&);
    core::Status<> on_list(const Message&);

    // ---- helpers ------------------------------------------------------------
    /// Send a numeric reply. The client's current nick (or "*" before
    /// registration) is ALWAYS injected as the implicit first parameter,
    /// mirroring the original irc_send_cmd(). @p params is everything that
    /// follows the nick on the wire; embed a leading ':' to start the trailer.
    core::Status<> send_numeric(int code, std::string_view params);

    /// Build a 353 RPL_NAMREPLY message for the given channel and member list.
    core::Status<> send_names_reply(std::string_view irc_channel,
                                    const std::vector<std::string>& member_nicks);

    /// Return the effective nick for error replies (uses "*" before registration).
    [[nodiscard]] std::string_view effective_nick() const noexcept {
        return nick_.empty() ? std::string_view{"*"} : std::string_view{nick_};
    }

    ISessionContext*                   ctx_;
    application::auth::LoginUser*      login_user_       = nullptr;

    /// Chat use-cases. Null when not wired (stub mode).
    std::shared_ptr<application::chat::ListChannels> list_channels_;
    std::shared_ptr<application::chat::JoinChannel>  join_channel_;
    std::shared_ptr<application::chat::PostMessage>  post_message_;
    std::shared_ptr<application::chat::LeaveChannel> leave_channel_;

    IrcState                           state_            = IrcState::Greeting;
    std::string                        nick_;
    std::string                        user_;
    std::optional<std::string>         pending_password_;  ///< stored by on_pass()
    std::string                        channel_;           ///< IRC channel name (with '#')
    std::string                        topic_;
    std::string                        away_msg_;  ///< non-empty ⇒ user is away

    /// Account ID resolved after successful login (0 until authenticated).
    domain::AccountId account_id_{0};

    /// Channel ID of the currently joined channel (0 until JOIN succeeds).
    domain::ChannelId channel_id_{0};

    /// Session ID for this connection (used in use-case calls).
    domain::SessionId session_id_{};
};

}  // namespace pvpgn::protocol::irc
