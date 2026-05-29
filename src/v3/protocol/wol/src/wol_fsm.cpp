// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/wol/wol_fsm.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

#include "application/auth/login_user.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "core/error.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::protocol::wol {

namespace {

// Maximum line length accepted from the client (IRC RFC 1459 limit).
constexpr std::size_t kMaxLineLen = 512;

/// ASCII upper-case a string in-place.
void to_upper(std::string& s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

/// Split a line into command and params.
/// IRC format: [:<prefix> ] COMMAND [params...] [:<trailing>]
/// We ignore the prefix (server-to-server) and just extract COMMAND + rest.
struct ParsedLine {
    std::string command;
    std::string params;  // everything after the command (raw)
};

ParsedLine parse_line(std::string_view line) {
    ParsedLine result;

    // Skip optional prefix ":something "
    std::size_t pos = 0;
    if (!line.empty() && line[0] == ':') {
        pos = line.find(' ');
        if (pos == std::string_view::npos) return result;
        pos++;  // skip the space
    }

    // Extract command (up to next space or end)
    std::size_t cmd_end = line.find(' ', pos);
    if (cmd_end == std::string_view::npos) {
        result.command = std::string(line.substr(pos));
    } else {
        result.command = std::string(line.substr(pos, cmd_end - pos));
        // params: everything after the space
        result.params = std::string(line.substr(cmd_end + 1));
    }

    to_upper(result.command);
    return result;
}

/// Extract the trailing parameter (after ':') from a params string.
/// e.g. "target :some text" → trailing = "some text"
std::string_view extract_trailing(std::string_view params) {
    auto colon = params.find(':');
    if (colon == std::string_view::npos) return {};
    return params.substr(colon + 1);
}

/// Extract the first token from params (space-delimited).
std::string_view first_token(std::string_view params) {
    auto sp = params.find(' ');
    if (sp == std::string_view::npos) return params;
    return params.substr(0, sp);
}

/// Trim leading/trailing whitespace.
std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.remove_suffix(1);
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------
// WolFsm::on_bytes
// ---------------------------------------------------------------------------

core::Status<> WolFsm::on_bytes(std::span<const std::byte> bytes) {
    if (state_ == WolState::Disconnecting) {
        return core::ok();
    }
    if (!ctx_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "wol fsm: no session context"});
    }

    // Append bytes to line buffer.
    for (auto b : bytes) {
        line_buf_ += static_cast<char>(b);
        // Guard against runaway clients.
        if (line_buf_.size() > kMaxLineLen * 4) {
            state_ = WolState::Disconnecting;
            ctx_->close();
            return core::fail(core::Error{core::StatusCode::ResourceExhausted,
                                          "wol fsm: line buffer overflow"});
        }
    }

    return process_lines();
}

// ---------------------------------------------------------------------------
// WolFsm::on_close
// ---------------------------------------------------------------------------

void WolFsm::on_close() {
    state_ = WolState::Disconnecting;
    line_buf_.clear();
}

// ---------------------------------------------------------------------------
// WolFsm::process_lines
// ---------------------------------------------------------------------------

core::Status<> WolFsm::process_lines() {
    while (true) {
        // Look for \r\n or bare \n.
        auto crlf = line_buf_.find("\r\n");
        auto lf   = line_buf_.find('\n');

        std::size_t line_end;
        std::size_t consume;

        if (crlf != std::string::npos &&
            (lf == std::string::npos || crlf <= lf)) {
            line_end = crlf;
            consume  = crlf + 2;
        } else if (lf != std::string::npos) {
            line_end = lf;
            consume  = lf + 1;
        } else {
            break;  // no complete line yet
        }

        std::string line = line_buf_.substr(0, line_end);
        line_buf_.erase(0, consume);

        if (line.empty()) continue;

        auto st = dispatch_line(line);
        if (!st) return st;

        if (state_ == WolState::Disconnecting) break;
    }
    return core::ok();
}

// ---------------------------------------------------------------------------
// WolFsm::dispatch_line
// ---------------------------------------------------------------------------

core::Status<> WolFsm::dispatch_line(std::string_view line) {
    auto [cmd, params] = parse_line(line);

    if (cmd.empty()) return core::ok();

    if (cmd == "NICK")    return on_nick(params);
    if (cmd == "USER")    return on_user(params);
    if (cmd == "PASS")    return on_pass(params);
    if (cmd == "PING")    return on_ping(params);
    if (cmd == "PONG")    return core::ok();  // ignore client PONGs
    if (cmd == "QUIT")    return on_quit(params);
    if (cmd == "LIST")    return on_list(params);
    if (cmd == "JOIN")    return on_join(params);
    if (cmd == "PART")    return on_part(params);
    if (cmd == "PRIVMSG") return on_privmsg(params);

    // WOL-specific commands that we acknowledge but don't fully implement yet.
    // CVERS, VERCHK, APGAR, SETOPT, SERIAL, GAMEOPT, STARTG, JOINGAME, etc.
    // Return 421 ERR_UNKNOWNCOMMAND for truly unknown commands.
    const std::string_view wol_known[] = {
        "CVERS", "VERCHK", "APGAR", "SETOPT", "SERIAL",
        "GAMEOPT", "STARTG", "JOINGAME", "FINDUSER", "FINDUSEREX",
        "PAGE", "ADVERTR", "ADVERTC", "CHANCHK", "GETBUDDY",
        "ADDBUDDY", "DELBUDDY", "HOST", "INVMSG", "INVDEL",
        "USERIP", "SQUADINFO", "CLANBYNAME", "SETCODEPAGE",
        "GETCODEPAGE", "SETLOCALE", "GETLOCALE", "GETINSIDER",
        "LISTSEARCH", "RUNGSEARCH", "HIGHSCORE", "NAMES",
        "TOPIC", "TIME", "KICK", "MODE",
    };
    for (auto kw : wol_known) {
        if (cmd == kw) return core::ok();  // silently accept
    }

    // 421 ERR_UNKNOWNCOMMAND
    return send_numeric(421, nick_.empty() ? "*" : nick_,
                        cmd + " :Unknown command");
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

core::Status<> WolFsm::on_nick(std::string_view params) {
    auto new_nick = trim(first_token(params));
    if (new_nick.empty()) {
        // 431 ERR_NONICKNAMEGIVEN
        return send_numeric(431, nick_.empty() ? "*" : nick_,
                            "No nickname given");
    }

    nick_ = std::string(new_nick);

    if (state_ == WolState::Connecting) {
        state_ = WolState::Authenticating;
    }
    return core::ok();
}

core::Status<> WolFsm::on_user(std::string_view params) {
    // USER <username> <hostname> <servername> :<realname>
    auto sp1 = params.find(' ');
    if (sp1 == std::string_view::npos) {
        return send_numeric(461, nick_.empty() ? "*" : nick_,
                            "USER :Not enough parameters");
    }
    user_ = std::string(params.substr(0, sp1));

    auto trailing = extract_trailing(params);
    realname_ = std::string(trailing.empty() ? user_ : trailing);

    // If PASS was already received before USER, attempt auth now that we
    // have all three credentials (NICK + USER + PASS).
    // close_on_bad_credentials=true: PASS-first ordering cannot retry.
    if (state_ == WolState::Authenticating && !nick_.empty() && !pass_.empty()) {
        return try_authenticate(/*close_on_bad_credentials=*/true);
    }

    return core::ok();
}

core::Status<> WolFsm::on_pass(std::string_view params) {
    // PASS <password>  (may be prefixed with ':')
    auto pw = trim(params);
    if (!pw.empty() && pw[0] == ':') pw.remove_prefix(1);
    pass_ = std::string(pw);

    // WOL auth: we need NICK + USER + PASS.
    // If we already have nick_ and user_, attempt auth immediately.
    // Otherwise store pass_ and wait for the remaining commands.
    if (state_ != WolState::Authenticating || nick_.empty() || user_.empty()) {
        return core::ok();
    }

    // Normal PASS-last ordering: on InvalidCredentials keep connection open
    // so the client can retry.
    return try_authenticate(/*close_on_bad_credentials=*/false);
}

core::Status<> WolFsm::try_authenticate(bool close_on_bad_credentials) {
    // R289: If a LoginUser use-case is wired in, perform real OLS auth.
    if (login_user_ != nullptr) {
        // Parse the username — reject malformed names immediately.
        auto name_result = domain::UserName::parse(nick_);
        if (!name_result) {
            // 432 ERR_ERRONEUSNICKNAME
            auto st = send_numeric(432, nick_, "Erroneous nickname");
            if (!st) return st;
            state_ = WolState::Disconnecting;
            ctx_->close();
            return core::ok();
        }

        // WOL sends the password in plain text.  We store it in a zeroed
        // BNHash and rely on the IPasswordHasher injected into LoginUser to
        // derive the expected hash from the stored hash1 and compare.
        // (The LoginWithSessionHashRequest path is used for BNCS OLS; for WOL
        // we use the simpler LoginRequest with a zeroed hash as a placeholder
        // until a WOL-specific hasher is wired in Phase 5.)
        //
        // C++20 designated initializers: UserName has no default ctor so we
        // must supply all fields in declaration order.
        application::auth::LoginRequest req{
            /* name               = */ std::move(name_result).value(),
            /* password_candidate = */ domain::BNHash{},    // zeroed — hasher re-derives
            /* tag                = */ domain::ClientTag{},  // WOL has no product tag
            /* ip                 = */ domain::IpAddress{},  // filled by infra layer
            /* session            = */ domain::SessionId{}   // filled by infra layer
        };

        auto result = login_user_->execute(std::move(req));

        if (!result) {
            // 464 ERR_PASSWDMISMATCH
            auto st = send_numeric(464, nick_, "Password incorrect");
            if (!st) return st;
            // Close on UnknownUser (hard failure) or when the caller
            // indicates that the ordering does not allow a retry.
            if (close_on_bad_credentials ||
                result.error() == application::auth::LoginError::UnknownUser) {
                state_ = WolState::Disconnecting;
                ctx_->close();
            }
            return core::ok();
        }

        // Auth succeeded.
        state_ = WolState::Authenticated;
        std::string welcome = "Welcome to WOL, ";
        welcome += nick_;
        auto st = send_numeric(1, nick_, welcome);
        if (!st) return st;
        std::string yourhost = "Your host is ";
        yourhost += std::string(ctx_->server_name());
        yourhost += ", running PvPGN v3";
        st = send_numeric(2, nick_, yourhost);
        if (!st) return st;
        st = send_numeric(375, nick_, "- Message of the day -");
        if (!st) return st;
        return send_numeric(376, nick_, "End of /MOTD command.");
    }

    // Skeleton / test mode: accept any non-empty nick+user combination.
    state_ = WolState::Authenticated;
    std::string welcome = "Welcome to WOL, ";
    welcome += nick_;
    auto st = send_numeric(1, nick_, welcome);
    if (!st) return st;
    std::string yourhost = "Your host is ";
    yourhost += std::string(ctx_->server_name());
    yourhost += ", running PvPGN v3";
    st = send_numeric(2, nick_, yourhost);
    if (!st) return st;
    st = send_numeric(375, nick_, "- Message of the day -");
    if (!st) return st;
    return send_numeric(376, nick_, "End of /MOTD command.");
}

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
    auto st = send_numeric(321, nick_, "Channel :Users  Name");
    if (!st) return st;

    // R299: relay via ListChannels use-case when available.
    if (list_channels_) {
        application::chat::ListChannelsRequest req;
        req.max_results   = 100;
        req.filter_by_tag = std::nullopt;

        auto result = list_channels_->execute(req);
        if (result) {
            for (const auto& info : result.value()) {
                // 322 RPL_LIST  <channel> <count> :<topic>
                std::string line = ":";
                line += std::string(ctx_->server_name());
                line += " 322 ";
                line += nick_;
                line += ' ';
                line += info.name;
                line += ' ';
                line += std::to_string(info.member_count);
                line += " :";
                // topic is not stored in ChannelInfo; send empty string
                if (auto s = send_raw(line); !s) return s;
            }
        }
        // On error fall through to RPL_LISTEND with whatever we sent.
    } else if (state_ == WolState::InChannel && !channel_.empty()) {
        // Stub / no use-case: emit a 322 entry for the channel we're in.
        std::string line = ":";
        line += std::string(ctx_->server_name());
        line += " 322 ";
        line += nick_;
        line += ' ';
        line += channel_;
        line += " 1 :";
        if (auto s = send_raw(line); !s) return s;
    }

    // 323 RPL_LISTEND
    return send_numeric(323, nick_, "End of /LIST");
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

    // R300: relay via JoinChannel use-case when available.
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

    // R300: channel message (target starts with '#').
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
            // Success: other members receive the message via event dispatch.
            // No echo back to sender required by WoL protocol.
            return core::ok();
        }
        // No use-case wired — silently accept (stub mode).
        return core::ok();
    }

    // R300: private message to a user — Phase H; send 401 ERR_NOSUCHNICK.
    std::string tgt{target};
    return send_numeric(401, nick_, tgt + " :No such nick");
}

// ---------------------------------------------------------------------------
// Reply helpers
// ---------------------------------------------------------------------------

core::Status<> WolFsm::send_numeric(int code,
                                     std::string_view target,
                                     std::string_view text) {
    // Format: ":server_name NNN target :text\r\n"
    char code_str[4];
    code_str[0] = static_cast<char>('0' + (code / 100) % 10);
    code_str[1] = static_cast<char>('0' + (code / 10)  % 10);
    code_str[2] = static_cast<char>('0' + code % 10);
    code_str[3] = '\0';

    std::string line = ":";
    line += ctx_->server_name();
    line += ' ';
    line.append(code_str, 3);
    line += ' ';
    line += target;
    line += " :";
    line += text;

    return send_raw(line);
}

core::Status<> WolFsm::send_raw(std::string_view line) {
    // Append \r\n and send.
    std::string buf(line);
    buf += "\r\n";
    return ctx_->send_bytes(
        std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(buf.data()),
            buf.size()
        });
}

}  // namespace pvpgn::protocol::wol
