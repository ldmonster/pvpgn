// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/irc/fsm.hpp"

#include <string>
#include <vector>

#include "application/auth/login_user.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "core/error.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/user_name.hpp"

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
// Registration
// ===========================================================================

core::Status<> IrcFsm::try_complete_registration() {
    if (nick_.empty() || user_.empty() || state_ != IrcState::Greeting)
        return core::ok();

    // When a LoginUser use-case is wired in, we also require a PASS before
    // completing registration.  If the password hasn't arrived yet, wait.
    if (login_user_ != nullptr && !pending_password_.has_value())
        return core::ok();

    if (login_user_ != nullptr) {
        // --- OLS authentication path (R290) ---
        auto name_result = domain::UserName::parse(nick_);
        if (!name_result) {
            // 432 ERR_ERRONEUSNICKNAME
            auto s = send_numeric(432, effective_nick(),
                                  nick_ + " :Erroneous nickname");
            if (!s) return s;
            state_ = IrcState::Closing;
            ctx_->close();
            return core::ok();
        }

        application::auth::LoginRequest req{
            std::move(name_result).value(),
            domain::BNHash{},
            domain::ClientTag{},
            domain::IpAddress{},
            domain::SessionId{}
        };

        auto result = login_user_->execute(std::move(req));
        if (!result) {
            // 464 ERR_PASSWDMISMATCH
            auto s = send_numeric(464, effective_nick(), "Password incorrect");
            if (!s) return s;
            state_ = IrcState::Closing;
            ctx_->close();
            return core::ok();
        }

        // Store the resolved account ID for use-case calls.
        account_id_ = result.value().id;
    }

    state_ = IrcState::Registered;
    // 001 RPL_WELCOME
    std::string text = "Welcome to PvPGN, ";
    text += nick_;
    return send_numeric(1, nick_, text);
}

// ---------------------------------------------------------------------------
// R290 — PASS handler
// ---------------------------------------------------------------------------

core::Status<> IrcFsm::on_pass(const Message& m) {
    // Silently ignore PASS after registration is complete.
    if (state_ != IrcState::Greeting)
        return core::ok();

    if (m.params.empty() || m.params[0].empty()) {
        // 461 ERR_NEEDMOREPARAMS
        return send_numeric(461, effective_nick(),
                            "PASS :Not enough parameters");
    }

    pending_password_ = m.params[0];

    // If NICK and USER have already been received, attempt registration now.
    return try_complete_registration();
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

// ---------------------------------------------------------------------------
// JOIN #channel
// ---------------------------------------------------------------------------

core::Status<> IrcFsm::on_join(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        // 451 ERR_NOTREGISTERED
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.empty() || m.params[0].empty()) {
        // 461 ERR_NEEDMOREPARAMS
        return send_numeric(461, nick_, "JOIN :Not enough parameters");
    }

    const std::string& irc_chan = m.params[0];  // e.g. "#Lobby"

    // Strip leading '#' for the domain channel name.
    std::string chan_name = irc_chan;
    if (!chan_name.empty() && chan_name[0] == '#') {
        chan_name.erase(0, 1);
    }

    // R301: relay via JoinChannel use-case when available.
    if (join_channel_) {
        auto join_result = join_channel_->execute(
            account_id_, chan_name, domain::ClientTag{});

        if (!join_result) {
            using E = application::chat::JoinChannelError;
            switch (join_result.error()) {
                case E::Banned:
                    // 474 ERR_BANNEDFROMCHAN
                    return send_numeric(474, nick_,
                                        irc_chan + " :Cannot join channel (+b)");
                case E::NotFound:
                case E::InvalidChannelName:
                default:
                    // 403 ERR_NOSUCHCHANNEL
                    return send_numeric(403, nick_,
                                        irc_chan + " :No such channel");
            }
        }

        // Store channel state.
        channel_    = irc_chan;  // keep '#' prefix for IRC display
        channel_id_ = join_result.value().channel.id();
        topic_      = join_result.value().channel.topic();
        state_      = IrcState::InChannel;

        // :<nick>!<nick>@pvpgn JOIN :#<channel>
        Message echo;
        echo.prefix  = nick_ + "!" + nick_ + "@pvpgn";
        echo.command = "JOIN";
        echo.params.emplace_back(channel_);
        auto s = ctx_->send(echo);
        if (!s) return s;

        // 332 RPL_TOPIC (or 331 RPL_NOTOPIC)
        if (topic_.empty()) {
            s = send_numeric(331, nick_, channel_ + " :No topic is set");
        } else {
            s = send_numeric(332, nick_, channel_ + " :" + topic_);
        }
        if (!s) return s;

        // 353 RPL_NAMREPLY — list of members from the channel snapshot.
        std::vector<std::string> member_nicks;
        for (const auto& mid : join_result.value().channel.member_ids()) {
            // We only have account IDs here; use numeric string as placeholder
            // until a nick-lookup service is wired in Phase H.
            member_nicks.push_back(std::to_string(mid.value()));
        }
        s = send_names_reply(channel_, member_nicks);
        if (!s) return s;

        // 366 RPL_ENDOFNAMES
        return send_numeric(366, channel_, "End of /NAMES list");
    }

    // Stub / no use-case: accept the join locally.
    channel_ = irc_chan;
    state_   = IrcState::InChannel;

    // Echo back the JOIN with our user prefix.
    Message echo;
    echo.prefix  = nick_ + "!" + nick_ + "@pvpgn";
    echo.command = "JOIN";
    echo.params.emplace_back(channel_);
    auto s = ctx_->send(echo);
    if (!s) return s;

    // 332 RPL_TOPIC (empty topic for stub)
    s = send_numeric(332, nick_, channel_ + " :");
    if (!s) return s;

    // 353 RPL_NAMREPLY — skeleton: just ourselves
    s = send_names_reply(channel_, {nick_});
    if (!s) return s;

    // 366 RPL_ENDOFNAMES
    return send_numeric(366, channel_, "End of /NAMES list");
}

// ---------------------------------------------------------------------------
// PART #channel [reason]
// ---------------------------------------------------------------------------

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

    const std::string reason = (m.params.size() >= 2) ? m.params[1] : "";

    // R301: relay via LeaveChannel use-case when available.
    if (leave_channel_ && channel_id_.value() != 0) {
        auto leave_result = leave_channel_->execute(channel_id_, account_id_);
        // On error, still proceed with local state cleanup (best-effort).
        (void)leave_result;
    }

    // Echo PART back to the client.
    Message echo;
    echo.prefix  = nick_ + "!" + nick_ + "@pvpgn";
    echo.command = "PART";
    echo.params.emplace_back(target_chan);
    if (!reason.empty()) {
        echo.params.emplace_back(reason);
    }
    auto s = ctx_->send(echo);
    if (!s) return s;

    channel_.clear();
    channel_id_ = domain::ChannelId{0};
    topic_.clear();
    state_ = IrcState::Registered;
    return core::ok();
}

// ---------------------------------------------------------------------------
// PRIVMSG <target> :<message>
// ---------------------------------------------------------------------------

core::Status<> IrcFsm::on_privmsg(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.size() < 2) {
        // 411 ERR_NORECIPIENT / no text
        return send_numeric(411, nick_, "PRIVMSG :No recipient or text");
    }

    const std::string& target  = m.params[0];
    const std::string& text    = m.params[1];

    // Channel message (target starts with '#').
    if (!target.empty() && target[0] == '#') {
        // R301: relay via PostMessage use-case when available.
        if (post_message_ && channel_id_.value() != 0) {
            auto chat_msg_result = domain::ChatMessage::create(text);
            if (!chat_msg_result) {
                // 404 ERR_CANNOTSENDTOCHAN
                return send_numeric(404, nick_,
                                    target + " :Cannot send to channel");
            }

            auto post_result = post_message_->execute(
                channel_id_, account_id_, chat_msg_result.value());

            if (!post_result) {
                // 404 ERR_CANNOTSENDTOCHAN
                return send_numeric(404, nick_,
                                    target + " :Cannot send to channel");
            }
            // Success: other members receive the message via event dispatch.
            // No echo back to sender required by IRC protocol.
            return core::ok();
        }

        // Stub / no use-case: silently accept.
        return core::ok();
    }

    // Private message to a nick — Phase H; send 401 ERR_NOSUCHNICK.
    return send_numeric(401, nick_, target + " :No such nick");
}

// ---------------------------------------------------------------------------
// NOTICE <target> :<message>
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// AWAY [message]
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// WHOIS <nick>
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// WHO <mask>
// ---------------------------------------------------------------------------

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
        reply352.params.emplace_back(std::string{ctx_->server_name()});
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

// ---------------------------------------------------------------------------
// MODE <target> [modes]
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// TOPIC #channel [new_topic]
// ---------------------------------------------------------------------------

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
        // SET topic — Phase H: no set_topic use-case yet; return 482.
        // 482 ERR_CHANOPRIVSNEEDED
        return send_numeric(482, nick_,
                            channel_ + " :You're not channel operator");
    }

    // GET topic
    if (topic_.empty()) {
        // 331 RPL_NOTOPIC
        return send_numeric(331, nick_, channel_ + " :No topic is set");
    }
    // 332 RPL_TOPIC
    return send_numeric(332, nick_, channel_ + " :" + topic_);
}

// ---------------------------------------------------------------------------
// NAMES [#channel]
// ---------------------------------------------------------------------------

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
        auto s = send_names_reply(channel_, {nick_});
        if (!s) return s;
    }

    // 366 RPL_ENDOFNAMES
    return send_numeric(366, target_chan, "End of /NAMES list.");
}

// ---------------------------------------------------------------------------
// KICK #channel <target> [reason]
// ---------------------------------------------------------------------------

core::Status<> IrcFsm::on_kick(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }
    if (m.params.size() < 2) {
        return send_numeric(461, nick_, "KICK :Not enough parameters");
    }

    const std::string& target_chan = m.params[0];

    if (state_ != IrcState::InChannel || channel_ != target_chan) {
        return send_numeric(403, nick_, target_chan + " :No such channel");
    }

    // Phase H: kick is not yet implemented; return 482 ERR_CHANOPRIVSNEEDED.
    return send_numeric(482, nick_,
                        channel_ + " :You're not channel operator");
}

// ---------------------------------------------------------------------------
// LIST [#channel_filter]
// ---------------------------------------------------------------------------

core::Status<> IrcFsm::on_list(const Message&) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, effective_nick(), "You have not registered");
    }

    // 321 RPL_LISTSTART
    auto s = send_numeric(321, nick_, "Channel :Users  Name");
    if (!s) return s;

    // R301: relay via ListChannels use-case when available.
    if (list_channels_) {
        application::chat::ListChannelsRequest req;
        req.max_results   = 100;
        req.filter_by_tag = std::nullopt;

        auto result = list_channels_->execute(req);
        if (result) {
            for (const auto& info : result.value()) {
                // 322 RPL_LIST: :<server> 322 <nick> #<channel> <count> :<topic>
                Message list_item;
                list_item.prefix  = std::string{ctx_->server_name()};
                list_item.command = "322";
                list_item.params.emplace_back(nick_);
                list_item.params.emplace_back("#" + info.name);
                list_item.params.emplace_back(std::to_string(info.member_count));
                // topic is not stored in ChannelInfo; send empty string
                list_item.params.emplace_back("");
                s = ctx_->send(list_item);
                if (!s) return s;
            }
        }
        // On error fall through to RPL_LISTEND with whatever we sent.
    } else {
        // Stub: emit the current channel if we're in one.
        if (state_ == IrcState::InChannel && !channel_.empty()) {
            Message list_item;
            list_item.prefix  = std::string{ctx_->server_name()};
            list_item.command = "322";
            list_item.params.emplace_back(nick_);
            list_item.params.emplace_back(channel_);
            list_item.params.emplace_back("1");  // user count
            list_item.params.emplace_back(topic_.empty() ? "" : topic_);
            s = ctx_->send(list_item);
            if (!s) return s;
        }
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
