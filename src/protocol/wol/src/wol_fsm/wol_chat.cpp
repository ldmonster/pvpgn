// SPDX-License-Identifier: GPL-2.0-or-later
/// @file wol_chat.cpp
/// WolFsm — post-authentication channel and chat command handlers.
///
///   on_ping()    — PING <token> → PONG :<token>
///   on_quit()    — QUIT → ERROR :Closing Link + close
///   on_list()    — LIST → 321 + 322 entries + 323
///   on_join()    — JOIN #channel → echo + 353 + 366
///   on_part()    — PART #channel → echo + state reset
///   on_privmsg() — PRIVMSG <target> :<text> → PostMessage use-case or stub

#include "protocol/wol/wol_fsm.hpp"

#include <cctype>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/set_channel_topic.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "application/game/wol_game_store.hpp"
#include "application/game/wol_user_flags_store.hpp"
#include "application/social/add_friend.hpp"
#include "application/social/list_friends.hpp"
#include "application/social/remove_friend.hpp"
#include "domain/chat/channel.hpp"
#include "domain/chat/ports.hpp"
#include "domain/connection/peer_address_store.hpp"
#include "domain/connection/ports.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "wol_fsm/wol_internal.hpp"

namespace pvpgn::protocol::wol {

core::Status<> WolFsm::on_ping(std::string_view params) {
    // PING [token]  →  ":<server> PONG <server>[ :<token>]"
    //
    // Mirrors the original irc.cpp _handle_ping_command + irc_send_pong: the
    // server replies with its own hostname as the PONG source AND as the first
    // PONG parameter, and only appends the client's token as a trailing param
    // when one was supplied. The original takes the FIRST middle param when the
    // argument is unprefixed, or the whole trailing text when it is ':'-prefixed;
    // a bare "PING" yields no trailing token at all (not a stray empty ":").
    auto rest = trim(params);
    std::string token;
    if (!rest.empty() && rest[0] == ':') {
        token = std::string(rest.substr(1));        // trailing: keep as-is
    } else if (!rest.empty()) {
        const auto sp = rest.find(' ');
        token = std::string(sp == std::string_view::npos ? rest
                                                         : rest.substr(0, sp));
    }

    const std::string sv{ctx_->server_name()};
    std::string pong = ":";
    pong += sv;
    pong += " PONG ";
    pong += sv;
    if (!token.empty()) {
        pong += " :";
        pong += token;
    }
    return send_raw(pong);
}

core::Status<> WolFsm::on_quit(std::string_view /*params*/) {
    state_ = WolState::Disconnecting;
    auto st = send_raw("ERROR :Closing Link");
    ctx_->close();
    return st;
}

core::Status<> WolFsm::on_list(std::string_view /*params*/) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    // 321 RPL_LISTSTART
    auto st = send_numeric(321, nick_, "Channel :Users Names");
    if (!st) return st;

    // WOL lists chat channels with RPL_CHANNEL (327), NOT the standard IRC 322:
    //   :server 327 nick <#name> <userCount> <official 0|1> 388
    // (WOLv2 ends the line with " 388"; WOLv1 with ":"). We emit the WOLv2 form.
    // The line is built with send_raw so the params follow the nick directly,
    // exactly like the original irc_send_cmd (no leading ':' before <#name>).
    //
    // The channel name is converted exactly like the original irc_convert_channel:
    // a leading '#' is prepended and the name is escaped (space -> '_', plus the
    // %-escapes for the reserved set) so the token is a single, valid IRC channel
    // name on the wire. Without this v3 emitted the bare store name, dropping the
    // '#' and leaving embedded spaces that split the token (e.g. "Diablo II").
    auto irc_channel_name = [](std::string_view name) -> std::string {
        if (!name.empty() && name.front() == '#') name.remove_prefix(1);
        std::string out;
        out.reserve(name.size() + 1);
        out += '#';
        for (char c : name) {
            switch (c) {
                case ' ':  out += '_';  break;
                case '_':  out += "%_"; break;
                case '%':  out += "%%"; break;
                case '\b': out += "%b"; break;
                case '\n': out += "%n"; break;
                case '\r': out += "%r"; break;
                case ':':  out += "%="; break;
                case ',':  out += "%-"; break;
                default:   out += c;    break;
            }
        }
        return out;
    };
    auto emit_channel = [&](std::string_view name, std::size_t count)
        -> core::Status<> {
        std::string line = ":";
        line += std::string(ctx_->server_name());
        line += " 327 ";
        line += nick_;
        line += ' ';
        line += irc_channel_name(name);
        line += ' ';
        line += std::to_string(count);
        line += " 0 388";  // 0 = user channel; 388 = WOLv2 line terminator
        return send_raw(line);
    };

    if (list_channels_) {
        application::chat::ListChannelsRequest req;
        req.max_results   = 100;
        req.filter_by_tag = std::nullopt;
        auto result = list_channels_->execute(req);
        if (result) {
            for (const auto& info : result.value()) {
                if (auto s = emit_channel(info.name, info.member_count); !s)
                    return s;
            }
        }
    } else if (state_ == WolState::InChannel && !channel_.empty()) {
        if (auto s = emit_channel(channel_, 1); !s) return s;
    }

    // 323 RPL_LISTEND
    return send_numeric(323, nick_, "End of LIST command");
}

core::Status<> WolFsm::on_names(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto chan_sv = trim(first_token(params));
    if (chan_sv.empty()) {
        // Bare NAMES lists every channel on the original; that set is config-
        // dependent and not differentially meaningful here. Just end the list.
        return send_numeric(366, std::string(nick_) + " *", "End of NAMES list");
    }

    std::string chan_disp{chan_sv};                 // keep the '#' for display
    std::string chan_name{chan_sv};
    if (!chan_name.empty() && chan_name[0] == '#') chan_name.erase(0, 1);

    // Build the member roster; the channel operator (tmpOP) gets an '@' prefix.
    std::string members;
    if (channel_reader_) {
        if (auto ch = channel_reader_->find_by_name(chan_name)) {
            const auto op = ch.value().operator_id();
            bool first = true;
            for (const auto& mid : ch.value().member_ids()) {
                std::string nm;
                if (auth_.account_reader) {
                    if (auto a = auth_.account_reader->find_by_id(mid)) {
                        nm = std::string(a.value().name().display());
                    }
                }
                if (nm.empty()) nm = std::to_string(mid.value());
                if (!first) members += ' ';
                first = false;
                if (op && op->value() == mid.value()) members += '@';
                members += nm;
            }
        }
    }

    // 353 RPL_NAMREPLY: ":server 353 <nick> * <channel> :<members>"
    if (auto s = send_numeric(353, std::string(nick_) + " * " + chan_disp,
                              members); !s) {
        return s;
    }
    // 366 RPL_ENDOFNAMES: ":server 366 <nick> <channel> :End of NAMES list"
    return send_numeric(366, std::string(nick_) + " " + chan_disp,
                        "End of NAMES list");
}

core::Status<> WolFsm::on_time() {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // 391 RPL_TIME: ":server 391 <nick> <server> :<unixtime>" — mirrors the
    // original's _handle_time_command (time(NULL)). The server name is repeated
    // as a param, then the unix time as the trailing arg.
    const std::string sv{ctx_->server_name()};
    const std::string target =
        std::string(nick_.empty() ? "*" : nick_) + " " + sv;
    return send_numeric(391, target,
                        std::to_string(static_cast<long long>(std::time(nullptr))));
}

core::Status<> WolFsm::on_mode(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto target = trim(first_token(params));
    if (target.empty()) {
        return send_needmoreparams("MODE");
    }
    // Channel mode query. The original returns a fixed "+tns" for a plain query
    // and an empty ban list (368) for "MODE #chan b". Mode *changes* route
    // through the operator commands and are not handled here.
    if (target[0] == '#') {
        // Second token, if any (e.g. the "b" ban-list query).
        auto rest = params;
        auto sp = rest.find(' ');
        std::string_view sub =
            (sp == std::string_view::npos) ? std::string_view{}
                                           : trim(rest.substr(sp + 1));
        if (!sub.empty() && (sub[0] == 'b' || sub == "+b")) {
            // 368 RPL_ENDOFBANLIST (empty ban list).
            return send_numeric(368, std::string(nick_) + " " + std::string(target),
                                "End of channel ban list");
        }
        // 324 RPL_CHANNELMODEIS: the mode is a plain param (no leading ':'), so
        // build the line directly rather than via send_numeric (which adds " :").
        std::string line = ":";
        line += ctx_->server_name();
        line += " 324 ";
        line += nick_;
        line += ' ';
        line += std::string(target);
        line += " +tns";
        return send_raw(line);
    }
    // User-mode query -> 501 ERR_UMODEUNKNOWNFLAG, matching the original.
    return send_numeric(501, nick_, "Unknown MODE flag");
}

core::Status<> WolFsm::on_kick(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // Parse "#chan <victim> [:reason]".
    auto chan_sv = trim(first_token(params));
    std::string_view rest;
    if (auto sp = params.find(' '); sp != std::string_view::npos) {
        rest = trim(params.substr(sp + 1));
    }
    auto victim_sv = trim(first_token(rest));
    if (chan_sv.empty() || victim_sv.empty()) {
        return send_needmoreparams("KICK");
    }
    // Reason: trailing text after the victim, ':' stripped; default "Bye".
    std::string reason;
    if (auto sp = rest.find(' '); sp != std::string_view::npos) {
        auto r = trim(rest.substr(sp + 1));
        if (!r.empty() && r[0] == ':') r.remove_prefix(1);
        reason = std::string(r);
    }
    if (reason.empty()) reason = "Bye";

    if (!channel_reader_ || !leave_channel_ || !auth_.account_reader ||
        channel_id_.value() == 0) {
        return send_numeric(442, nick_, std::string(chan_sv) + " :You're not on that channel");
    }
    auto ch = channel_reader_->find_by_id(channel_id_);
    if (!ch) {
        return send_numeric(442, nick_, std::string(chan_sv) + " :You're not on that channel");
    }
    // Only the channel operator (tmpOP, wave 58) may kick.
    const auto op = ch.value().operator_id();
    if (!op || op->value() != account_id_.value()) {
        return send_numeric(482, nick_,
                            std::string(chan_sv) + " :You're not channel operator");
    }
    // Resolve the victim and confirm membership.
    auto vparsed = domain::UserName::parse(std::string(victim_sv));
    if (!vparsed) {
        return send_numeric(441, nick_, std::string(victim_sv) + " " +
                            std::string(chan_sv) + " :They aren't on that channel");
    }
    auto vacct = auth_.account_reader->find_by_name(vparsed.value());
    if (!vacct || !ch.value().contains(vacct.value().id())) {
        return send_numeric(441, nick_, std::string(victim_sv) + " " +
                            std::string(chan_sv) + " :They aren't on that channel");
    }
    const auto victim_id = vacct.value().id();
    const std::string victim_name{vacct.value().name().display()};

    // Capture every member's session (including the victim and the kicker) BEFORE
    // removal so they all receive the KICK broadcast.
    auto recipients = current_channel_member_sessions(/*exclude_self=*/false);

    // Remove the victim from the channel.
    (void)leave_channel_->execute(channel_id_, victim_id);

    // Broadcast the KICK. The original prefixes the line with the victim's
    // hostmask; we use the v3-consistent "@Battle.net" form.
    std::string line = ":";
    line += victim_name;
    line += '!';
    line += victim_name;
    line += "@Battle.net KICK ";
    line += channel_;
    line += ' ';
    line += victim_name;
    line += " :";
    line += reason;
    route_irc_line(line, recipients);
    return core::ok();
}

core::Status<> WolFsm::on_topic(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto chan_sv = trim(first_token(params));
    if (chan_sv.empty()) {
        return send_needmoreparams("TOPIC");
    }
    const std::string chan_disp{chan_sv};  // keep '#'

    // Topic text, if supplied (everything after the channel token; ':' stripped).
    bool has_topic = false;
    std::string new_topic;
    if (auto sp = params.find(' '); sp != std::string_view::npos) {
        auto rest = trim(params.substr(sp + 1));
        if (!rest.empty()) {
            has_topic = true;
            if (rest[0] == ':') rest.remove_prefix(1);
            new_topic = std::string(rest);
        }
    }

    if (has_topic) {
        // SET: persist via the use-case (member-gated, <=255 chars), then echo
        // 332 RPL_TOPIC to the setter — exactly what the original does.
        if (set_channel_topic_ && channel_id_.value() != 0) {
            application::chat::SetChannelTopicRequest req{
                account_id_, channel_id_, new_topic};
            (void)set_channel_topic_->execute(req);
        }
        return send_numeric(332, std::string(nick_) + " " + chan_disp, new_topic);
    }

    // QUERY: reply 332 with the stored topic (empty if unset). The original
    // CRASHES on this path (NULL deref); v3 handles it safely.
    std::string topic;
    if (channel_reader_ && channel_id_.value() != 0) {
        if (auto ch = channel_reader_->find_by_id(channel_id_)) {
            topic = ch.value().topic();
        }
    }
    return send_numeric(332, std::string(nick_) + " " + chan_disp, topic);
}

core::Status<> WolFsm::on_join(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    auto chan_sv = trim(first_token(params));
    if (chan_sv.empty()) {
        return send_needmoreparams("JOIN");
    }

    // Strip leading '#' for the domain channel name.
    std::string chan_name{chan_sv};
    if (!chan_name.empty() && chan_name[0] == '#') {
        chan_name.erase(0, 1);
    }

    // Relay via JoinChannel use-case when available.
    if (join_channel_) {
        auto join_result = join_channel_->execute(
            account_id_, chan_name, domain::ClientTag{});

        if (!join_result) {
            // 403 ERR_NOSUCHCHANNEL
            return send_numeric(403, nick_,
                                chan_name + " :No such channel");
        }

        // Store channel state.
        channel_    = std::string(chan_sv);  // keep '#' prefix for IRC display
        channel_id_ = join_result.value().channel.id();
        state_      = WolState::InChannel;

        // :<nick>!<nick>@Battle.net JOIN :<channel>
        std::string join_echo = ":";
        join_echo += nick_;
        join_echo += "!";
        join_echo += nick_;
        join_echo += "@Battle.net JOIN :";
        join_echo += channel_;
        if (auto s = send_raw(join_echo); !s) return s;

        // 353 RPL_NAMREPLY  = <nick> = <channel> :<member1> <member2> ...
        std::string names_line = ":";
        names_line += std::string(ctx_->server_name());
        names_line += " 353 ";
        names_line += nick_;
        names_line += " = ";
        names_line += channel_;
        names_line += " :";
        // List member names from the channel snapshot.
        bool first = true;
        for (const auto& mid : join_result.value().channel.member_ids()) {
            if (!first) names_line += ' ';
            first = false;
            // We only have account IDs here; use numeric string as placeholder.
            names_line += std::to_string(mid.value());
        }
        if (auto s = send_raw(names_line); !s) return s;

        // 332 RPL_TOPIC: the original sends the channel topic on every join
        // (empty when unset), so a joiner sees a topic set by an earlier member.
        if (auto s = send_numeric(332, std::string(nick_) + " " + channel_,
                                  join_result.value().channel.topic()); !s) {
            return s;
        }

        // 366 RPL_ENDOFNAMES
        return send_numeric(366, channel_, "End of /NAMES list");
    }

    // Stub / no use-case: accept the join locally.
    channel_ = std::string(chan_sv);
    state_   = WolState::InChannel;

    // Echo JOIN back with user prefix.
    std::string join_echo = ":";
    join_echo += nick_;
    join_echo += "!";
    join_echo += nick_;
    join_echo += "@Battle.net JOIN :";
    join_echo += channel_;
    auto st = send_raw(join_echo);
    if (!st) return st;

    // 353 RPL_NAMREPLY  = <nick> = <channel> :<nick>
    {
        std::string names_line = ":";
        names_line += std::string(ctx_->server_name());
        names_line += " 353 ";
        names_line += nick_;
        names_line += " = ";
        names_line += channel_;
        names_line += " :";
        names_line += nick_;
        if (auto s = send_raw(names_line); !s) return s;
    }

    // 332 RPL_TOPIC (empty topic for stub)
    st = send_numeric(332, channel_, "");
    if (!st) return st;

    // 366 RPL_ENDOFNAMES
    return send_numeric(366, channel_, "End of /NAMES list");
}

core::Status<> WolFsm::on_part(std::string_view /*params*/) {
    // The original's _handle_part_command IGNORES its parameters entirely: it
    // calls conn_part_channel(conn), which parts the connection's CURRENT
    // channel (and resets an open game). When the client is not on a channel it
    // is a silent no-op — NO error numeric is sent (v3 previously replied 442,
    // and previously echoed the *parameter* channel instead of the real one).
    if (state_ != WolState::InChannel && state_ != WolState::InGame) {
        return core::ok();
    }

    // The IRC PART line names the actual current channel (v3-consistent
    // "@Battle.net" hostmask; the original uses the client's WCHT@ip form, which
    // is environment-dependent).
    std::string line = ":";
    line += nick_;
    line += '!';
    line += nick_;
    line += "@Battle.net PART ";
    line += channel_;

    // Leave the domain channel and notify the remaining members, mirroring the
    // original's channel_del_connection(message_type_part). (v3 previously only
    // echoed to the parting user and never left the domain channel, so other
    // members never saw the PART and the leaver ghosted in the roster.)
    if (leave_channel_ && account_id_.value() != 0 && channel_id_.value() != 0) {
        if (auto result = leave_channel_->execute(channel_id_, account_id_)) {
            route_irc_line(line, result.value().members_to_notify);
        }
    }

    // Echo the PART back to the parting user (the original broadcasts to the
    // whole channel, the leaver included).
    if (auto st = send_raw(line); !st) return st;

    channel_.clear();
    channel_id_ = domain::ChannelId{0};
    state_ = WolState::Authenticated;
    return core::ok();
}

core::Status<> WolFsm::on_privmsg(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    // PRIVMSG <target> :<message>
    auto sp = params.find(' ');
    if (sp == std::string_view::npos) {
        return send_numeric(411, nick_, "No recipient given (PRIVMSG)");
    }

    std::string_view target  = params.substr(0, sp);
    std::string_view rest    = params.substr(sp + 1);
    // Strip leading ':' from the trailing parameter.
    std::string_view message = rest;
    if (!message.empty() && message[0] == ':') message.remove_prefix(1);

    if (message.empty()) {
        return send_numeric(412, nick_, "No text to send");
    }

    // Channel message (target starts with '#').
    if (!target.empty() && target[0] == '#') {
        if (post_message_) {
            // Strip '#' to get the bare channel name for the domain.
            auto chat_msg_result = domain::ChatMessage::create(std::string{message});
            if (!chat_msg_result) {
                // 404 ERR_CANNOTSENDTOCHAN
                std::string chan{target};
                return send_numeric(404, nick_,
                                    chan + " :Cannot send to channel");
            }

            auto post_result = post_message_->execute(
                channel_id_, account_id_, chat_msg_result.value());

            if (!post_result) {
                std::string chan{target};
                return send_numeric(404, nick_,
                                    chan + " :Cannot send to channel");
            }

            // Relay to every other channel member's connection. WOL clients
            // expect the standard IRC form, with the sender's hostmask prefix:
            //   :<nick>!<nick>@<host> PRIVMSG <#channel> :<message>
            // The line is identical for all recipients (it carries the
            // sender's identity), so encode once and route to each SessionId
            // PostMessage resolved. The sender gets no echo (matches the
            // original WOL server).
            std::string line = ":";
            line += nick_;
            line += '!';
            line += nick_;
            line += "@Battle.net PRIVMSG ";
            line += std::string(target);
            line += " :";
            line += std::string(message);
            route_irc_line(line, post_result.value().recipients);
            return core::ok();
        }
        // No use-case wired — silently accept (stub mode).
        return core::ok();
    }

    // Private message to a user; send 401 ERR_NOSUCHNICK.
    std::string tgt{target};
    return send_numeric(401, nick_, tgt + " :No such nick");
}

void WolFsm::route_irc_line(const std::string& line,
                            const std::vector<domain::SessionId>& recipients) {
    if (!message_router_ || recipients.empty()) return;
    std::string buf = line;
    buf += "\r\n";
    const auto* bytes = reinterpret_cast<const std::byte*>(buf.data());
    std::span<const std::byte> payload{bytes, buf.size()};
    for (const auto& sid : recipients) {
        (void)message_router_->send(sid, payload);
    }
}

std::vector<domain::SessionId>
WolFsm::current_channel_member_sessions(bool exclude_self) const {
    std::vector<domain::SessionId> sessions;
    if (!channel_reader_ || !auth_.session_registry || channel_id_.value() == 0) {
        return sessions;
    }
    auto channel = channel_reader_->find_by_id(channel_id_);
    if (!channel) return sessions;
    for (const auto& mid : channel.value().member_ids()) {
        if (exclude_self && mid.value() == account_id_.value()) continue;
        if (auto sid = auth_.session_registry->session_for(mid)) {
            sessions.push_back(sid.value());
        }
    }
    return sessions;
}

core::Status<> WolFsm::on_gameopt(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    // GAMEOPT <target> :<gameOptions>
    auto sp = params.find(' ');
    if (sp == std::string_view::npos) {
        return send_needmoreparams("GAMEOPT");
    }
    std::string_view target  = params.substr(0, sp);
    std::string_view message = params.substr(sp + 1);
    if (!message.empty() && message[0] == ':') message.remove_prefix(1);
    if (target.empty() || message.empty()) {
        return send_needmoreparams("GAMEOPT");
    }

    // The relayed line carries the sender's identity and the opaque options
    // text, exactly like the original `message_type_gameopt_*`:
    //   :<nick>!<nick>@<host> GAMEOPT <target> :<gameOptions>
    std::string line = ":";
    line += nick_;
    line += '!';
    line += nick_;
    line += "@Battle.net GAMEOPT ";
    line += std::string(target);
    line += " :";
    line += std::string(message);

    if (target[0] == '#') {
        // Channel game-options: broadcast to the current channel's members
        // (the original keys off conn_get_channel, not the target name). No
        // self-echo. Mirrors channel_message_send(message_type_gameopt_talk).
        route_irc_line(line, current_channel_member_sessions(/*exclude_self=*/true));
        return core::ok();
    }

    // User game-options: whisper to a single nick. Resolve nick -> account ->
    // session; 401 if the target is not a known/online user.
    if (auth_.account_reader && auth_.session_registry) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            if (acct) {
                if (auto sid = auth_.session_registry->session_for(acct.value().id())) {
                    route_irc_line(line, {sid.value()});
                    return core::ok();
                }
            }
        }
    }
    std::string tgt{target};
    return send_numeric(401, nick_, tgt + " :No such nick");
}

namespace {
/// Split on runs of spaces into non-empty tokens (IRC-style param list).
std::vector<std::string_view> split_ws(std::string_view s) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') ++i;
        std::size_t start = i;
        while (i < s.size() && s[i] != ' ') ++i;
        if (i > start) out.push_back(s.substr(start, i - start));
    }
    return out;
}
}  // namespace

core::Status<> WolFsm::on_joingame(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    auto tok = split_ws(params);
    if (tok.empty()) {
        return send_needmoreparams("JOINGAME");
    }

    // The first token is the game/channel name (kept with '#' for the wire ack,
    // stripped for the domain channel/game name).
    std::string raw_name{tok[0]};
    std::string game_name = raw_name;
    if (!game_name.empty() && game_name[0] == '#') game_name.erase(0, 1);
    if (game_name.empty() || !join_channel_) {
        return send_needmoreparams("JOINGAME");
    }

    auto joingame_prefix = [&](std::string_view text) {
        std::string line = ":";
        line += nick_;
        line += '!';
        line += nick_;
        line += "@Battle.net JOINGAME ";
        line += text;
        return line;
    };

    // ----- CREATE mode: JOINGAME #name min max type a b tournament [ext] [pass]
    if (tok.size() >= 7) {
        auto u = [&](std::size_t i) -> std::uint32_t {
            return i < tok.size()
                       ? static_cast<std::uint32_t>(
                             std::strtoul(std::string{tok[i]}.c_str(), nullptr, 10))
                       : 0u;
        };
        auto join_result = join_channel_->execute(account_id_, game_name,
                                                  domain::ClientTag{});
        if (!join_result) {
            return send_numeric(478, nick_, raw_name + " :JOINGAME failed");
        }
        channel_      = raw_name;
        channel_id_   = join_result.value().channel.id();
        state_        = WolState::InChannel;

        if (wol_game_store_) {
            application::game::WolGameInfo info;
            info.name           = game_name;
            info.min_players    = u(1);
            info.max_players    = u(2);
            info.game_type      = u(3);
            info.tournament     = u(6);
            info.game_extension = tok.size() >= 8 ? std::string{tok[7]} : "0";
            info.password       = tok.size() >= 9 ? std::string{tok[8]} : "";
            info.host           = account_id_;
            info.channel_id     = channel_id_;
            info.players        = {account_id_};
            wol_game_store_->create(info);
        }

        // WOLv1 create ack (numparams==7): "<min> <max> <type> <p4> 0 <tourn> :#name"
        std::string text;
        text += std::string{tok[1]};
        text += ' ';
        text += std::string{tok[2]};
        text += ' ';
        text += std::string{tok[3]};
        text += ' ';
        text += std::string{tok[4]};
        text += " 0 ";
        text += std::string{tok[6]};
        text += " :";
        text += raw_name;
        // CREATE acks the host only.
        return send_raw(joingame_prefix(text));
    }

    // ----- JOIN mode: JOINGAME #name <something> [password]
    if (tok.size() == 2 || tok.size() == 3) {
        if (!wol_game_store_) {
            return send_numeric(478, nick_, raw_name + " :Game channel has closed");
        }
        auto game = wol_game_store_->find(game_name);
        if (!game) {
            return send_numeric(478, nick_, raw_name + " :Game channel has closed");
        }
        if (game->is_full()) {
            return send_numeric(471, nick_, raw_name + " :Channel is full.");
        }
        if (!game->password.empty()) {
            std::string supplied = tok.size() == 3 ? std::string{tok[2]} : "";
            if (supplied != game->password) {
                return send_numeric(475, nick_, raw_name + " :Bad password");
            }
        }

        auto join_result = join_channel_->execute(account_id_, game_name,
                                                  domain::ClientTag{});
        if (!join_result) {
            return send_numeric(478, nick_, raw_name + " :JOINGAME failed");
        }
        channel_    = raw_name;
        channel_id_ = join_result.value().channel.id();
        state_      = WolState::InChannel;
        (void)wol_game_store_->add_player(game_name, account_id_);

        // WOLv1 join ack: "<min> <max> <type> 1 1 <tourn> :#channel".
        std::string text;
        text += std::to_string(game->min_players);
        text += ' ';
        text += std::to_string(game->max_players);
        text += ' ';
        text += std::to_string(game->game_type);
        text += " 1 1 ";
        text += std::to_string(game->tournament);
        text += " :";
        text += raw_name;
        // JOIN acks every channel member (the original channel_message_sends it),
        // so the host learns a player joined and the joiner gets its ack.
        route_irc_line(joingame_prefix(text),
                       current_channel_member_sessions(/*exclude_self=*/false));
        return core::ok();
    }

    return send_needmoreparams("JOINGAME");
}

core::Status<> WolFsm::on_finduser(std::string_view params, bool ex) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    auto target = trim(first_token(params));
    if (target.empty()) {
        return send_needmoreparams(ex ? "FINDUSEREX" : "FINDUSER");
    }

    // Default: not found / not findable.  Found ("0") iff the target account is
    // online (a live session); WOL `findme` defaults on, so online == findable.
    // The payload carries the user's current channel (empty if none).
    std::string payload = "1 :";
    if (auth_.account_reader && auth_.session_registry) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            const bool findable =
                acct && auth_.session_registry->session_for(acct.value().id()) &&
                (!user_flags_store_ ||
                 user_flags_store_->get(acct.value().id()).findme);
            if (findable) {
                std::string chan;
                if (channel_reader_) {
                    const auto id = acct.value().id();
                    channel_reader_->forEach(
                        [&](const domain::chat::Channel& c) {
                            for (const auto& mid : c.member_ids()) {
                                if (mid.value() == id.value()) {
                                    chan = "#" + c.name();
                                    return false;  // stop iteration
                                }
                            }
                            return true;
                        });
                }
                payload = ex ? ("0 :" + chan + ",0") : ("0 :" + chan);
            }
        }
    }

    // Wire form mirrors irc_send_cmd (payload verbatim, carries its own ':').
    return send_raw_cmd(ex ? 398 : 388, payload);
}

// Build ":<server> <code> <nick> <params>" — the irc_send_cmd framing, with the
// params copied verbatim (no injected ':').
core::Status<> WolFsm::send_raw_cmd(int code, std::string_view params) {
    char code_str[3];
    code_str[0] = static_cast<char>('0' + (code / 100) % 10);
    code_str[1] = static_cast<char>('0' + (code / 10) % 10);
    code_str[2] = static_cast<char>('0' + code % 10);
    std::string line = ":";
    line += std::string(ctx_->server_name());
    line += ' ';
    line.append(code_str, 3);
    line += ' ';
    line += nick_;
    line += ' ';
    line += params;
    return send_raw(line);
}

core::Status<> WolFsm::on_getbuddy() {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // Backtick-terminated buddy list (matches the original 333 payload).
    std::string list;
    if (list_friends_) {
        auto r = list_friends_->execute(account_id_);
        if (r) {
            for (const auto& f : r.value()) {
                list += std::string(f.name.display());
                list += '`';
            }
        }
    }
    return send_raw_cmd(333, list);
}

core::Status<> WolFsm::on_addbuddy(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto target = trim(first_token(params));
    if (target.empty()) {
        return send_needmoreparams("ADDBUDDY");
    }
    if (add_friend_ && auth_.account_reader) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            if (acct) {
                (void)add_friend_->execute(account_id_, acct.value().id());
                return send_raw_cmd(334, target);
            }
        }
    }
    // Unknown account: 401 ERR_NOSUCHNICK (the original sends "<name> :No such nick").
    return send_numeric(401, nick_, std::string(target) + " :No such nick");
}

core::Status<> WolFsm::on_delbuddy(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto target = trim(first_token(params));
    if (target.empty()) {
        return send_needmoreparams("DELBUDDY");
    }
    if (remove_friend_ && auth_.account_reader) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            if (acct) {
                (void)remove_friend_->execute(account_id_, acct.value().id());
            }
        }
    }
    // The original echoes 335 with the name regardless of whether it was present.
    return send_raw_cmd(335, target);
}

namespace {
/// Case-insensitive equality for nick comparison.
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}
}  // namespace

core::Status<> WolFsm::on_setcodepage(std::string_view params) {
    auto cp = trim(first_token(params));
    if (cp.empty()) return send_needmoreparams("SETCODEPAGE");
    codepage_ = std::atoi(std::string{cp}.c_str());
    return send_raw_cmd(329, cp);
}

core::Status<> WolFsm::on_getcodepage(std::string_view params) {
    auto tok = split_ws(params);
    if (tok.empty()) return send_needmoreparams("GETCODEPAGE");
    // "<nick>`<cp>`<nick>`<cp>" — own codepage for our nick, 0 for others
    // (v3 has no cross-session codepage registry).
    std::string payload;
    for (std::size_t i = 0; i < tok.size(); ++i) {
        if (i) payload += '`';
        const int cp = iequals(tok[i], nick_) ? codepage_ : 0;
        payload += std::string{tok[i]};
        payload += '`';
        payload += std::to_string(cp);
    }
    return send_raw_cmd(328, payload);
}

core::Status<> WolFsm::on_setlocale(std::string_view params) {
    auto loc = trim(first_token(params));
    if (loc.empty()) return send_needmoreparams("SETLOCALE");
    locale_ = std::atoi(std::string{loc}.c_str());
    return send_raw_cmd(310, loc);
}

core::Status<> WolFsm::on_getlocale(std::string_view params) {
    auto tok = split_ws(params);
    if (tok.empty()) return send_needmoreparams("GETLOCALE");
    std::string payload;
    for (std::size_t i = 0; i < tok.size(); ++i) {
        if (i) payload += '`';
        const int loc = iequals(tok[i], nick_) ? locale_ : 0;
        payload += std::string{tok[i]};
        payload += '`';
        payload += std::to_string(loc);
    }
    return send_raw_cmd(309, payload);
}

core::Status<> WolFsm::on_getinsider(std::string_view params) {
    auto target = trim(first_token(params));
    if (target.empty()) {
        return send_needmoreparams("GETINSIDER");
    }
    return send_raw_cmd(399, std::string{target} + "`0");
}

core::Status<> WolFsm::on_page(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // PAGE <target> :<message>
    auto sp = params.find(' ');
    if (sp == std::string_view::npos) {
        return send_needmoreparams("PAGE");
    }
    std::string_view target  = params.substr(0, sp);
    std::string_view message = params.substr(sp + 1);
    if (!message.empty() && message[0] == ':') message.remove_prefix(1);
    if (target.empty() || message.empty()) {
        return send_needmoreparams("PAGE");
    }

    // Deliver the page to the target if it resolves to an online account
    // (pageme defaults on, so online == pageable). "PAGE 0" battleclan broadcast
    // is not modelled. Reply 389 "0 :" when paged, "1 :" otherwise.
    bool paged = false;
    if (target != "0" && message_router_ && auth_.account_reader &&
        auth_.session_registry) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            const bool pageable =
                acct && (!user_flags_store_ ||
                         user_flags_store_->get(acct.value().id()).pageme);
            if (pageable) {
                if (auto sid = auth_.session_registry->session_for(
                        acct.value().id())) {
                    std::string line = ":";
                    line += nick_;
                    line += '!';
                    line += nick_;
                    line += "@Battle.net PAGE :";
                    line += std::string(message);
                    route_irc_line(line, {sid.value()});
                    paged = true;
                }
            }
        }
    }
    return send_raw_cmd(389, paged ? "0 :" : "1 :");
}

core::Status<> WolFsm::on_chanchk(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto chan = trim(first_token(params));
    if (chan.empty()) return core::ok();  // original: no reply without a param

    std::string bare{chan};
    if (!bare.empty() && bare[0] == '#') bare.erase(0, 1);
    bool exists = false;
    if (channel_reader_) {
        if (auto r = channel_reader_->find_by_name(bare)) exists = true;
    }
    if (exists) {
        // ":<server> CHANCHK <channel>" (no nick — preformat uses a null source).
        std::string line = ":";
        line += std::string(ctx_->server_name());
        line += " CHANCHK ";
        line += std::string(chan);
        return send_raw(line);
    }
    return send_numeric(403, nick_, std::string(chan) + " :No such channel");
}

core::Status<> WolFsm::on_host(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // HOST <nick> [:<text>]
    auto sp = params.find(' ');
    std::string_view target = sp == std::string_view::npos
                                  ? params : params.substr(0, sp);
    std::string_view text = sp == std::string_view::npos
                                ? std::string_view{} : params.substr(sp + 1);
    if (!text.empty() && text[0] == ':') text.remove_prefix(1);
    target = trim(target);
    // The original (_handle_host_command) replies 461 when no nick is given.
    if (target.empty()) return send_needmoreparams("HOST");

    if (message_router_ && auth_.account_reader && auth_.session_registry) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            if (acct) {
                if (auto sid = auth_.session_registry->session_for(
                        acct.value().id())) {
                    // ":<nick>!<nick>@Battle.net HOST : <text>" (original sends
                    // the text prefixed with ": ").
                    std::string line = ":";
                    line += nick_;
                    line += '!';
                    line += nick_;
                    line += "@Battle.net HOST : ";
                    line += std::string(text);
                    route_irc_line(line, {sid.value()});
                    return core::ok();
                }
            }
        }
    }
    return send_numeric(401, nick_, std::string(target) + " :No such nick");
}

core::Status<> WolFsm::on_userip(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto target = trim(first_token(params));
    // The original (_handle_userip_command) replies 461 when no nick is given.
    if (target.empty()) return send_needmoreparams("USERIP");

    if (auth_.account_reader && peer_store_) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            if (acct) {
                if (auto ip = peer_store_->get(acct.value().id())) {
                    // Reply to the requester (original message_send_text(conn,...)):
                    // ":<nick>!<nick>@Battle.net USERIP <nick> <ip>".
                    std::string line = ":";
                    line += nick_;
                    line += '!';
                    line += nick_;
                    line += "@Battle.net USERIP ";
                    line += std::string(target);
                    line += ' ';
                    line += ip.value();
                    return send_raw(line);
                }
            }
        }
    }
    return send_numeric(401, nick_, std::string(target) + " :No such nick");
}

core::Status<> WolFsm::on_invmsg(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // INVMSG <channel> <flag> <invited,invited2,...>
    auto tok = split_ws(params);
    // The original (_handle_invmsg_command) replies 461 when fewer than 3 params.
    if (tok.size() < 3) return send_needmoreparams("INVMSG");

    const std::string chan_flag =
        std::string{tok[0]} + " " + std::string{tok[1]};  // "<channel> <flag>"

    if (!message_router_ || !auth_.account_reader || !auth_.session_registry) {
        return core::ok();
    }
    // The invited list is comma-separated; deliver to each online invitee. The
    // original's wire form carries the invitee's OWN name first (inserted by its
    // postformat): ":<sender>!.. INVMSG <invited> <channel> <flag>".
    std::string_view invited = tok[2];
    std::size_t pos = 0;
    while (pos <= invited.size()) {
        std::size_t comma = invited.find(',', pos);
        std::string_view who = invited.substr(
            pos, comma == std::string_view::npos ? std::string_view::npos
                                                 : comma - pos);
        who = trim(who);
        if (!who.empty()) {
            auto name = domain::UserName::parse(std::string{who});
            if (name) {
                auto acct = auth_.account_reader->find_by_name(name.value());
                if (acct) {
                    if (auto sid = auth_.session_registry->session_for(
                            acct.value().id())) {
                        std::string line = ":";
                        line += nick_;
                        line += '!';
                        line += nick_;
                        line += "@Battle.net INVMSG ";
                        line += std::string(who);
                        line += ' ';
                        line += chan_flag;
                        route_irc_line(line, {sid.value()});
                    }
                }
            }
        }
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    return core::ok();
}

core::Status<> WolFsm::on_advertr(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    auto chan = trim(first_token(params));
    if (chan.empty()) {
        return send_needmoreparams("ADVERTR");
    }
    // ":<server> ADVERTR 5 <channel>" (no nick — the original uses a null source).
    std::string line = ":";
    line += std::string(ctx_->server_name());
    line += " ADVERTR 5 ";
    line += std::string(chan);
    return send_raw(line);
}

core::Status<> WolFsm::on_setopt(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // SETOPT <find>,<page>  (16/17 = find off/on, 32/33 = page off/on). No reply.
    auto arg = trim(first_token(params));
    if (arg.empty() || !user_flags_store_ || account_id_.value() == 0) {
        return core::ok();
    }
    auto comma = arg.find(',');
    if (comma == std::string_view::npos) return core::ok();
    std::string_view find_opt = trim(arg.substr(0, comma));
    std::string_view page_opt = trim(arg.substr(comma + 1));
    application::game::WolUserFlags flags;
    flags.findme = (find_opt == "17");
    flags.pageme = (page_opt == "33");
    user_flags_store_->set(account_id_, flags);
    return core::ok();
}

core::Status<> WolFsm::on_startg(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    // STARTG <channel> <nick1,nick2,...>
    auto tok = split_ws(params);
    if (tok.size() < 2) {
        return send_needmoreparams("STARTG");
    }

    // The sender must own a game; resolve it from the game store by channel name.
    if (!wol_game_store_ || !message_router_ || !auth_.account_reader ||
        !auth_.session_registry) {
        return core::ok();
    }
    std::string game_name{tok[0]};
    if (!game_name.empty() && game_name[0] == '#') game_name.erase(0, 1);
    auto game = wol_game_store_->find(game_name);
    if (!game) return core::ok();  // original: "conn has not game" -> no reply

    // Owner IP (WOLv1 STARTG carries the game owner's address) + game id + time.
    std::string owner_ip;
    if (peer_store_) {
        if (auto ip = peer_store_->get(game->host)) owner_ip = ip.value();
    }
    const std::string tail = owner_ip + " " +
        std::to_string(game->channel_id.value()) + " " +
        std::to_string(static_cast<long long>(std::time(nullptr)));

    // Deliver STARTG to each named (comma-separated) player. The recipient's own
    // name fills the dest slot (the original's postformat), then the trailing
    // ":" payload carries owner_ip gameid time.
    std::string_view players = tok[1];
    std::size_t pos = 0;
    while (pos <= players.size()) {
        std::size_t comma = players.find(',', pos);
        std::string_view who = players.substr(
            pos, comma == std::string_view::npos ? std::string_view::npos
                                                 : comma - pos);
        who = trim(who);
        if (!who.empty()) {
            auto name = domain::UserName::parse(std::string{who});
            if (name) {
                auto acct = auth_.account_reader->find_by_name(name.value());
                if (acct) {
                    if (auto sid = auth_.session_registry->session_for(
                            acct.value().id())) {
                        std::string line = ":";
                        line += nick_;
                        line += '!';
                        line += nick_;
                        line += "@Battle.net STARTG ";
                        line += std::string(who);
                        line += " :";
                        line += tail;
                        route_irc_line(line, {sid.value()});
                    }
                }
            }
        }
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    return core::ok();
}

}  // namespace pvpgn::protocol::wol
