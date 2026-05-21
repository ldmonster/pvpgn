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
/// Commands handled by the FSM (Phase-4 skeleton):
///   * NICK / USER  — registration handshake; emits 001 RPL_WELCOME.
///   * PING         — answered immediately with PONG.
///   * JOIN         — transitions to InChannel; echoes JOIN + 366 RPL_ENDOFNAMES.
///   * PART         — leaves channel; echoes PART; transitions back to Registered.
///   * PRIVMSG      — echoes back (Phase-5 dispatcher will forward to use-case).
///   * NOTICE       — echoes back (same skeleton as PRIVMSG).
///   * QUIT         — closes session.
///   * AWAY         — sets/clears away status; replies 305/306.
///   * WHOIS <nick> — returns 311 RPL_WHOISUSER + 318 RPL_ENDOFWHOIS (self only).
///   * WHO  <mask>  — returns 352 RPL_WHOREPLY + 315 RPL_ENDOFWHO.
///   * MODE         — returns 324 RPL_CHANNELMODEIS (no-op skeleton).
///   * TOPIC        — returns 332 RPL_TOPIC or 331 RPL_NOTOPIC.
///   * NAMES        — returns 353 RPL_NAMREPLY + 366 RPL_ENDOFNAMES.
///   * KICK         — echoes KICK (no real enforcement in skeleton).
///   * MOTD         — returns 375 RPL_MOTDSTART + 376 RPL_ENDOFMOTD.
///   * LIST         — returns 321 RPL_LISTSTART + 323 RPL_LISTEND.
///
/// Error replies:
///   * 401 ERR_NOSUCHNICK   — WHOIS/WHO for unknown nick.
///   * 403 ERR_NOSUCHCHANNEL — channel commands with no channel.
///   * 411 ERR_NORECIPIENT  — PRIVMSG/NOTICE with missing params.
///   * 421 ERR_UNKNOWNCOMMAND — unrecognised command.
///   * 431 ERR_NONICKNAMEGIVEN — NICK with empty param.
///   * 451 ERR_NOTREGISTERED — commands requiring registration.
///   * 461 ERR_NEEDMOREPARAMS — commands with too few params.
///
/// Things deferred to Phase 5:
///   * Real auth, multi-channel, real MODE/TOPIC persistence.
///   * Cross-FSM hand-off (IRC ⇄ BNet bridging for D2/WCG bots).

#include <cstdint>
#include <string>

#include "core/result.hpp"
#include "protocol/irc/message.hpp"
#include "protocol/irc/session_context.hpp"

namespace pvpgn::protocol::irc {

enum class IrcState : std::uint8_t {
    Greeting,    ///< need NICK + USER
    Registered,  ///< handshake done
    InChannel,   ///< joined exactly one channel (skeleton limit)
    Closing,
};

class IrcFsm {
public:
    explicit IrcFsm(ISessionContext& ctx) noexcept : ctx_(&ctx) {}

    [[nodiscard]] IrcState         state()    const noexcept { return state_; }
    [[nodiscard]] std::string_view nick()     const noexcept { return nick_; }
    [[nodiscard]] std::string_view user()     const noexcept { return user_; }
    [[nodiscard]] std::string_view channel()  const noexcept { return channel_; }
    [[nodiscard]] std::string_view away_msg() const noexcept { return away_msg_; }
    [[nodiscard]] bool             is_away()  const noexcept { return !away_msg_.empty(); }

    /// Drive the FSM with one decoded inbound message.
    core::Status<> handle(const Message& msg);

private:
    // ---- registration -------------------------------------------------------
    core::Status<> on_nick(const Message&);
    core::Status<> on_user(const Message&);
    core::Status<> try_complete_registration();

    // ---- always-available ---------------------------------------------------
    core::Status<> on_ping(const Message&);
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
    core::Status<> send_numeric(int code,
                                std::string_view target,
                                std::string_view text);

    /// Return the effective nick for error replies (uses "*" before registration).
    [[nodiscard]] std::string_view effective_nick() const noexcept {
        return nick_.empty() ? std::string_view{"*"} : std::string_view{nick_};
    }

    ISessionContext* ctx_;
    IrcState         state_    = IrcState::Greeting;
    std::string      nick_;
    std::string      user_;
    std::string      channel_;
    std::string      topic_;
    std::string      away_msg_;  ///< non-empty ⇒ user is away
};

}  // namespace pvpgn::protocol::irc
