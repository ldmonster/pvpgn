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
/// Things the FSM does itself (Phase-4 skeleton):
///   * Responds to PING immediately.
///   * Tracks the nick/user pair and emits the canonical
///     `001 RPL_WELCOME` once both are supplied.
///   * Tracks the joined channel name (one at a time for the skeleton).
///   * Rejects PRIVMSG before registration with `451 ERR_NOTREGISTERED`.
///   * Rejects unknown commands with `421 ERR_UNKNOWNCOMMAND`.
///
/// Things deferred to Phase 5:
///   * Real auth, MOTD streaming, multi-channel, MODE/TOPIC, NAMES.
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

    IrcState           state()    const noexcept { return state_; }
    std::string_view   nick()     const noexcept { return nick_; }
    std::string_view   user()     const noexcept { return user_; }
    std::string_view   channel()  const noexcept { return channel_; }

    /// Drive the FSM with one decoded inbound message.
    core::Status<> handle(const Message& msg);

private:
    core::Status<> on_nick(const Message&);
    core::Status<> on_user(const Message&);
    core::Status<> on_ping(const Message&);
    core::Status<> on_join(const Message&);
    core::Status<> on_privmsg(const Message&);
    core::Status<> on_quit(const Message&);

    core::Status<> send_numeric(int code,
                                std::string_view target,
                                std::string_view text);
    core::Status<> try_complete_registration();

    ISessionContext* ctx_;
    IrcState         state_ = IrcState::Greeting;
    std::string      nick_;
    std::string      user_;
    std::string      channel_;
};

}  // namespace pvpgn::protocol::irc
