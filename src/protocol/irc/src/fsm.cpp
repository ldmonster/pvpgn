// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm.cpp
/// IrcFsm — thin coordinator.
///
/// This translation unit owns:
///   - make_numeric()       — RFC 1459 numeric-reply builder (shared helper,
///                            declared in fsm/irc_internal.hpp)
///   - send_numeric()       — convenience wrapper around make_numeric + ctx_->send
///   - send_names_reply()   — 353 RPL_NAMREPLY builder
///   - handle()             — main command dispatch
///
/// All handler implementations live in the focused sub-TUs under fsm/:
///   fsm/irc_registration.cpp — try_complete_registration, on_pass, on_nick, on_user
///   fsm/irc_commands.cpp     — on_ping, on_quit, on_motd, on_join, on_part,
///                              on_privmsg, on_notice, on_away, on_whois, on_who,
///                              on_mode, on_topic, on_names, on_kick, on_list

#include "protocol/irc/fsm.hpp"

#include <string>
#include <vector>

#include "core/error.hpp"
#include "fsm/irc_internal.hpp"

namespace pvpgn::protocol::irc {

// ===========================================================================
// Shared helper — defined here, declared in fsm/irc_internal.hpp
// ===========================================================================

Message make_numeric(std::string_view server, int code,
                     std::string_view target, std::string_view text) {
    Message m;
    m.prefix = std::string{server};
    char buf[4];  // 3 digits + NUL
    buf[0] = static_cast<char>('0' + (code / 100) % 10);
    buf[1] = static_cast<char>('0' + (code / 10) % 10);
    buf[2] = static_cast<char>('0' + code % 10);
    buf[3] = '\0';
    m.command.assign(buf, 3);
    m.params.emplace_back(target);
    m.params.emplace_back(text);
    return m;
}

// ===========================================================================
// Helpers
// ===========================================================================

core::Status<> IrcFsm::send_numeric(int code,
                                     std::string_view target,
                                     std::string_view text) {
    return ctx_->send(make_numeric(ctx_->server_name(), code, target, text));
}

core::Status<> IrcFsm::send_names_reply(std::string_view irc_channel,
                                         const std::vector<std::string>& member_nicks) {
    // 353 RPL_NAMREPLY: :<server> 353 <nick> = <channel> :<member1> <member2> ...
    Message names_reply;
    names_reply.prefix  = std::string{ctx_->server_name()};
    names_reply.command = "353";
    names_reply.params.emplace_back(nick_);
    names_reply.params.emplace_back("=");
    names_reply.params.emplace_back(irc_channel);

    // Build the space-separated member list.
    std::string members_str;
    bool first = true;
    for (const auto& m : member_nicks) {
        if (!first) members_str += ' ';
        first = false;
        members_str += m;
    }
    names_reply.params.emplace_back(std::move(members_str));

    return ctx_->send(names_reply);
}

// ===========================================================================
// Main dispatch
// ===========================================================================

core::Status<> IrcFsm::handle(const Message& msg) {
    if (state_ == IrcState::Closing) {
        return core::fail(core::Error{
            core::StatusCode::FailedPrecondition, "irc fsm: closing"});
    }

    const auto& c = msg.command;

    // Commands available in all non-Closing states.
    if (c == "PING")  return on_ping(msg);
    if (c == "QUIT")  return on_quit(msg);
    if (c == "MOTD")  return on_motd(msg);

    // Registration commands.
    if (c == "PASS")  return on_pass(msg);
    if (c == "NICK")  return on_nick(msg);
    if (c == "USER")  return on_user(msg);

    // Post-registration commands.
    if (c == "JOIN")    return on_join(msg);
    if (c == "PART")    return on_part(msg);
    if (c == "PRIVMSG") return on_privmsg(msg);
    if (c == "NOTICE")  return on_notice(msg);
    if (c == "AWAY")    return on_away(msg);
    if (c == "WHOIS")   return on_whois(msg);
    if (c == "WHO")     return on_who(msg);
    if (c == "MODE")    return on_mode(msg);
    if (c == "TOPIC")   return on_topic(msg);
    if (c == "NAMES")   return on_names(msg);
    if (c == "KICK")    return on_kick(msg);
    if (c == "LIST")    return on_list(msg);

    // 421 ERR_UNKNOWNCOMMAND
    return send_numeric(421, effective_nick(),
                        c + " :Unknown command");
}

}  // namespace pvpgn::protocol::irc
