// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/irc/fsm.hpp"

#include <string>

#include "core/error.hpp"

namespace pvpgn::protocol::irc {

// ===========================================================================
// Internal helpers
// ===========================================================================

namespace {

/// Build a numeric-reply Message following RFC 1459:
///   :<server> <NNN> <target> :<text>
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

}  // namespace

// ===========================================================================
// Helpers
// ===========================================================================

core::Status<> IrcFsm::send_numeric(int code,
                                     std::string_view target,
                                     std::string_view text) {
    return ctx_->send(make_numeric(ctx_->server_name(), code, target, text));
}

// ===========================================================================
// Registration
// ===========================================================================

core::Status<> IrcFsm::try_complete_registration() {
    if (!nick_.empty() && !user_.empty() && state_ == IrcState::Greeting) {
        state_ = IrcState::Registered;
        // 001 RPL_WELCOME
        std::string text = "Welcome to PvPGN, ";
        text += nick_;
        return send_numeric(1, nick_, text);
    }
    return core::ok();
}

core::Status<> IrcFsm::on_nick(const Message& m) {
    if (m.params.empty() || m.params[0].empty()) {
        // 431 ERR_NONICKNAMEGIVEN
        return send_numeric(431, effective_nick(), "No nickname given");
    }
    nick_ = m.params[0];
    return try_complete_registration();
}

core::Status<> IrcFsm::on_user(const Message& m) {
    // USER <user> <mode> <unused> :<realname>
    if (m.params.size() < 4) {
        // 461 ERR_NEEDMOREPARAMS
        return send_numeric(461, effective_nick(),
                            "USER :Not enough parameters");
    }
    user_ = m.params[0];
    return try_complete_registration();
}

// ===========================================================================
// Always-available commands (work in any non-Closing state)
// ===========================================================================

core::Status<> IrcFsm::on_ping(const Message& m) {
    Message pong;
    pong.prefix  = std::string{ctx_->server_name()};
    pong.command = "PONG";
    pong.params.emplace_back(ctx_->server_name());
    if (!m.params.empty()) {
        pong.params.emplace_back(m.params[0]);
    }
    return ctx_->send(pong);
}

core::Status<> IrcFsm::on_quit(const Message&) {
    state_ = IrcState::Closing;
    ctx_->close();
    return core::ok();
}

core::Status<> IrcFsm::on_motd(const Message&) {
    // 375 RPL_MOTDSTART
    auto s = send_numeric(375, nick_,
                          "- " + std::string{ctx_->server_name()} + " Message of the day -");
    if (!s) return s;
    // 376 RPL_ENDOFMOTD
    return send_numeric(376, nick_, "End of /MOTD command.");
}

// ===========================================================================
// Post-registration commands
// ===========================================================================

core::Status<> IrcFsm::on_join(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        // 451 ERR_NOTREGISTERED
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.empty() || m.params[0].empty()) {
        // 461 ERR_NEEDMOREPARAMS
        return send_numeric(461, nick_, "JOIN :Not enough parameters");
    }
    channel_ = m.params[0];
    state_   = IrcState::InChannel;

    // Echo back the JOIN with our user prefix.
    Message echo;
    echo.prefix  = nick_;
    echo.command = "JOIN";
    echo.params.emplace_back(channel_);
    auto s = ctx_->send(echo);
    if (!s) return s;

    // 353 RPL_NAMREPLY — list of members (skeleton: just ourselves)
    // Format: :<server> 353 <nick> = <channel> :<nick>
    Message names_reply;
    names_reply.prefix  = std::string{ctx_->server_name()};
    names_reply.command = "353";
    names_reply.params.emplace_back(nick_);
    names_reply.params.emplace_back("=");
    names_reply.params.emplace_back(channel_);
    names_reply.params.emplace_back(nick_);
    s = ctx_->send(names_reply);
    if (!s) return s;

    // 366 RPL_ENDOFNAMES
    return send_numeric(366, channel_, "End of /NAMES list.");
}

core::Status<> IrcFsm::on_part(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.empty() || m.params[0].empty()) {
        return send_numeric(461, nick_, "PART :Not enough parameters");
    }

    const std::string& target_chan = m.params[0];

    // If we're not in a channel, or the channel doesn't match, 403.
    if (state_ != IrcState::InChannel || channel_ != target_chan) {
        return send_numeric(403, nick_,
                            target_chan + " :No such channel");
    }

    // Echo PART back to the client.
    Message echo;
    echo.prefix  = nick_;
    echo.command = "PART";
    echo.params.emplace_back(target_chan);
    if (m.params.size() >= 2) {
        echo.params.emplace_back(m.params[1]);  // part message
    }
    auto s = ctx_->send(echo);
    if (!s) return s;

    channel_.clear();
    state_ = IrcState::Registered;
    return core::ok();
}

core::Status<> IrcFsm::on_privmsg(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.size() < 2) {
        // 411 ERR_NORECIPIENT / no text
        return send_numeric(411, nick_, "PRIVMSG :No recipient or text");
    }
    // Phase-5 dispatcher will forward to chat use-case. Skeleton echoes back
    // with the sender's nick as prefix so tests can observe the path.
    Message echo;
    echo.prefix  = nick_;
    echo.command = "PRIVMSG";
    echo.params  = m.params;
    return ctx_->send(echo);
}

core::Status<> IrcFsm::on_notice(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.size() < 2) {
        return send_numeric(411, nick_, "NOTICE :No recipient or text");
    }
    // Echo back with sender prefix (same skeleton as PRIVMSG).
    Message echo;
    echo.prefix  = nick_;
    echo.command = "NOTICE";
    echo.params  = m.params;
    return ctx_->send(echo);
}

core::Status<> IrcFsm::on_away(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.empty() || m.params[0].empty()) {
        // AWAY with no message → unset away
        away_msg_.clear();
        // 305 RPL_UNAWAY
        return send_numeric(305, nick_, "You are no longer marked as being away");
    }
    away_msg_ = m.params[0];
    // 306 RPL_NOWAWAY
    return send_numeric(306, nick_, "You have been marked as being away");
}

core::Status<> IrcFsm::on_whois(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.empty() || m.params[0].empty()) {
        return send_numeric(431, nick_, "No nickname given");
    }

    const std::string& target = m.params[0];

    // Skeleton: only knows about ourselves.
    if (target != nick_) {
        // 401 ERR_NOSUCHNICK
        return send_numeric(401, nick_, target + " :No such nick/channel");
    }

    // 311 RPL_WHOISUSER: <nick> <user> <host> * :<realname>
    Message reply311;
    reply311.prefix  = std::string{ctx_->server_name()};
    reply311.command = "311";
    reply311.params.emplace_back(nick_);
    reply311.params.emplace_back(nick_);
    reply311.params.emplace_back(user_.empty() ? nick_ : user_);
    reply311.params.emplace_back(ctx_->server_name());
    reply311.params.emplace_back("*");
    reply311.params.emplace_back(nick_);  // realname
    auto s = ctx_->send(reply311);
    if (!s) return s;

    // 318 RPL_ENDOFWHOIS
    return send_numeric(318, nick_, target + " :End of /WHOIS list.");
}

core::Status<> IrcFsm::on_who(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }

    const std::string mask = m.params.empty() ? "*" : m.params[0];

    // Skeleton: only knows about ourselves; emit one 352 if mask matches.
    if (mask == "*" || mask == nick_ ||
        (!channel_.empty() && mask == channel_)) {
        // 352 RPL_WHOREPLY: <channel> <user> <host> <server> <nick> H :0 <realname>
        Message reply352;
        reply352.prefix  = std::string{ctx_->server_name()};
        reply352.command = "352";
        reply352.params.emplace_back(nick_);
        reply352.params.emplace_back(channel_.empty() ? "*" : channel_);
        reply352.params.emplace_back(user_.empty() ? nick_ : user_);
        reply352.params.emplace_back(ctx_->server_name());
        reply352.params.emplace_back(std::string{ctx_->server_name()});
        reply352.params.emplace_back(nick_);
        reply352.params.emplace_back("H");  // H = here, G = gone (away)
        reply352.params.emplace_back("0 " + nick_);  // hopcount + realname
        auto s = ctx_->send(reply352);
        if (!s) return s;
    }

    // 315 RPL_ENDOFWHO
    return send_numeric(315, nick_, mask + " :End of /WHO list.");
}

core::Status<> IrcFsm::on_mode(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.empty()) {
        return send_numeric(461, nick_, "MODE :Not enough parameters");
    }

    const std::string& target = m.params[0];

    // Skeleton: return current channel mode string (empty = no modes set).
    // 324 RPL_CHANNELMODEIS: <channel> <mode string>
    return send_numeric(324, nick_, target + " +");
}

core::Status<> IrcFsm::on_topic(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.empty()) {
        return send_numeric(461, nick_, "TOPIC :Not enough parameters");
    }

    const std::string& target_chan = m.params[0];

    if (state_ != IrcState::InChannel || channel_ != target_chan) {
        return send_numeric(403, nick_, target_chan + " :No such channel");
    }

    if (m.params.size() >= 2) {
        // SET topic
        topic_ = m.params[1];
        // Echo TOPIC back to client
        Message echo;
        echo.prefix  = nick_;
        echo.command = "TOPIC";
        echo.params.emplace_back(channel_);
        echo.params.emplace_back(topic_);
        return ctx_->send(echo);
    }

    // GET topic
    if (topic_.empty()) {
        // 331 RPL_NOTOPIC
        return send_numeric(331, nick_, channel_ + " :No topic is set");
    }
    // 332 RPL_TOPIC
    return send_numeric(332, nick_, channel_ + " :" + topic_);
}

core::Status<> IrcFsm::on_names(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }

    const std::string target_chan =
        (m.params.empty() || m.params[0].empty())
            ? channel_
            : m.params[0];

    if (target_chan.empty()) {
        return send_numeric(461, nick_, "NAMES :Not enough parameters");
    }

    if (state_ == IrcState::InChannel && channel_ == target_chan) {
        // 353 RPL_NAMREPLY — skeleton: just ourselves
        Message names_reply;
        names_reply.prefix  = std::string{ctx_->server_name()};
        names_reply.command = "353";
        names_reply.params.emplace_back(nick_);
        names_reply.params.emplace_back("=");
        names_reply.params.emplace_back(channel_);
        names_reply.params.emplace_back(nick_);
        auto s = ctx_->send(names_reply);
        if (!s) return s;
    }

    // 366 RPL_ENDOFNAMES
    return send_numeric(366, target_chan, "End of /NAMES list.");
}

core::Status<> IrcFsm::on_kick(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.size() < 2) {
        return send_numeric(461, nick_, "KICK :Not enough parameters");
    }

    const std::string& target_chan = m.params[0];
    const std::string& target_nick = m.params[1];

    if (state_ != IrcState::InChannel || channel_ != target_chan) {
        return send_numeric(403, nick_, target_chan + " :No such channel");
    }

    // Skeleton: echo KICK back (no real enforcement).
    Message echo;
    echo.prefix  = nick_;
    echo.command = "KICK";
    echo.params.emplace_back(target_chan);
    echo.params.emplace_back(target_nick);
    if (m.params.size() >= 3) {
        echo.params.emplace_back(m.params[2]);  // kick reason
    }
    return ctx_->send(echo);
}

core::Status<> IrcFsm::on_list(const Message&) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }

    // 321 RPL_LISTSTART
    auto s = send_numeric(321, nick_, "Channel :Users  Name");
    if (!s) return s;

    // Skeleton: emit the current channel if we're in one.
    if (state_ == IrcState::InChannel && !channel_.empty()) {
        Message list_item;
        list_item.prefix  = std::string{ctx_->server_name()};
        list_item.command = "322";  // RPL_LIST
        list_item.params.emplace_back(nick_);
        list_item.params.emplace_back(channel_);
        list_item.params.emplace_back("1");  // user count
        list_item.params.emplace_back(topic_.empty() ? "" : topic_);
        s = ctx_->send(list_item);
        if (!s) return s;
    }

    // 323 RPL_LISTEND
    return send_numeric(323, nick_, "End of /LIST");
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
