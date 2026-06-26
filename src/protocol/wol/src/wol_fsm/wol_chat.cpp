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

#include <string>

#include "application/chat/join_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "domain/connection/ports.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "wol_fsm/wol_internal.hpp"

namespace pvpgn::protocol::wol {

core::Status<> WolFsm::on_ping(std::string_view params) {
    // PING <token>  →  PONG :<token>
    auto token = trim(params);
    if (!token.empty() && token[0] == ':') token.remove_prefix(1);

    std::string pong = "PONG :";
    pong += token;
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
    //   :server 327 nick <name> <userCount> <official 0|1> 388
    // (WOLv2 ends the line with " 388"; WOLv1 with ":"). We emit the WOLv2 form.
    // The line is built with send_raw so the params follow the nick directly,
    // exactly like the original irc_send_cmd (no leading ':' before <name>).
    auto emit_channel = [&](std::string_view name, std::size_t count)
        -> core::Status<> {
        std::string line = ":";
        line += std::string(ctx_->server_name());
        line += " 327 ";
        line += nick_;
        line += ' ';
        line += name;
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

core::Status<> WolFsm::on_join(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    auto chan_sv = trim(first_token(params));
    if (chan_sv.empty()) {
        return send_numeric(461, nick_, "JOIN :Not enough parameters");
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

core::Status<> WolFsm::on_part(std::string_view params) {
    if (state_ != WolState::InChannel && state_ != WolState::InGame) {
        return send_numeric(442, nick_, "You're not on that channel");
    }

    auto chan = trim(first_token(params));
    if (chan.empty()) chan = channel_;

    // Echo PART back.
    std::string part_echo = ":";
    part_echo += nick_;
    part_echo += " PART ";
    part_echo += chan;
    auto st = send_raw(part_echo);
    if (!st) return st;

    channel_.clear();
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
            // sender's identity), so encode once and route to each SessionId.
            // The sender gets no echo (matches the original WOL server).
            if (message_router_) {
                std::string line = ":";
                line += nick_;
                line += '!';
                line += nick_;
                line += "@Battle.net PRIVMSG ";
                line += std::string(target);
                line += " :";
                line += std::string(message);
                line += "\r\n";
                const auto* bytes =
                    reinterpret_cast<const std::byte*>(line.data());
                std::span<const std::byte> payload{bytes, line.size()};
                for (const auto& sid : post_result.value().recipients) {
                    (void)message_router_->send(sid, payload);
                }
            }
            return core::ok();
        }
        // No use-case wired — silently accept (stub mode).
        return core::ok();
    }

    // Private message to a user; send 401 ERR_NOSUCHNICK.
    std::string tgt{target};
    return send_numeric(401, nick_, tgt + " :No such nick");
}

}  // namespace pvpgn::protocol::wol
