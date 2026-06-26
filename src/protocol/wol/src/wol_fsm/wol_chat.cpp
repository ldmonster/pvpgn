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
#include <string>
#include <vector>

#include "application/chat/join_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "application/game/wol_game_store.hpp"
#include "application/social/add_friend.hpp"
#include "application/social/list_friends.hpp"
#include "application/social/remove_friend.hpp"
#include "domain/chat/channel.hpp"
#include "domain/chat/ports.hpp"
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
        return send_numeric(461, nick_, "GAMEOPT :Not enough parameters");
    }
    std::string_view target  = params.substr(0, sp);
    std::string_view message = params.substr(sp + 1);
    if (!message.empty() && message[0] == ':') message.remove_prefix(1);
    if (target.empty() || message.empty()) {
        return send_numeric(461, nick_, "GAMEOPT :Not enough parameters");
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
        return send_numeric(461, nick_, "JOINGAME :Not enough parameters");
    }

    // The first token is the game/channel name (kept with '#' for the wire ack,
    // stripped for the domain channel/game name).
    std::string raw_name{tok[0]};
    std::string game_name = raw_name;
    if (!game_name.empty() && game_name[0] == '#') game_name.erase(0, 1);
    if (game_name.empty() || !join_channel_) {
        return send_numeric(461, nick_, "JOINGAME :Not enough parameters");
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

    return send_numeric(461, nick_, "JOINGAME :Not enough parameters");
}

core::Status<> WolFsm::on_finduser(std::string_view params, bool ex) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    auto target = trim(first_token(params));
    if (target.empty()) {
        return send_numeric(461, nick_,
                            std::string(ex ? "FINDUSEREX" : "FINDUSER") +
                                " :Not enough parameters");
    }

    // Default: not found / not findable.  Found ("0") iff the target account is
    // online (a live session); WOL `findme` defaults on, so online == findable.
    // The payload carries the user's current channel (empty if none).
    std::string payload = "1 :";
    if (auth_.account_reader && auth_.session_registry) {
        auto name = domain::UserName::parse(std::string{target});
        if (name) {
            auto acct = auth_.account_reader->find_by_name(name.value());
            if (acct &&
                auth_.session_registry->session_for(acct.value().id())) {
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
        return send_numeric(461, nick_, "ADDBUDDY :Not enough parameters");
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
        return send_numeric(461, nick_, "DELBUDDY :Not enough parameters");
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
    if (cp.empty()) return core::ok();  // original: no reply without a param
    codepage_ = std::atoi(std::string{cp}.c_str());
    return send_raw_cmd(329, cp);
}

core::Status<> WolFsm::on_getcodepage(std::string_view params) {
    auto tok = split_ws(params);
    if (tok.empty()) return core::ok();  // original: no reply without a param
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
    if (loc.empty()) return core::ok();
    locale_ = std::atoi(std::string{loc}.c_str());
    return send_raw_cmd(310, loc);
}

core::Status<> WolFsm::on_getlocale(std::string_view params) {
    auto tok = split_ws(params);
    if (tok.empty()) return core::ok();
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
        return send_numeric(461, nick_, "GETINSIDER :Not enough parameters");
    }
    return send_raw_cmd(399, std::string{target} + "`0");
}

}  // namespace pvpgn::protocol::wol
