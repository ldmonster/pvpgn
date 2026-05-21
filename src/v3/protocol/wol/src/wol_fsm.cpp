// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/wol/wol_fsm.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "core/error.hpp"

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

    return core::ok();
}

core::Status<> WolFsm::on_pass(std::string_view params) {
    // PASS <password>  (may be prefixed with ':')
    auto pw = trim(params);
    if (!pw.empty() && pw[0] == ':') pw.remove_prefix(1);
    pass_ = std::string(pw);

    // WOL auth: we need NICK + USER + PASS.
    // For this skeleton we accept any non-empty nick+user combination.
    if (state_ == WolState::Authenticating && !nick_.empty() && !user_.empty()) {
        state_ = WolState::Authenticated;
        // 001 RPL_WELCOME
        std::string welcome = "Welcome to WOL, ";
        welcome += nick_;
        auto st = send_numeric(1, nick_, welcome);
        if (!st) return st;
        // 002 RPL_YOURHOST
        std::string yourhost = "Your host is ";
        yourhost += std::string(ctx_->server_name());
        yourhost += ", running PvPGN v3";
        st = send_numeric(2, nick_, yourhost);
        if (!st) return st;
        // 375 RPL_MOTDSTART + 376 RPL_ENDOFMOTD (minimal)
        st = send_numeric(375, nick_, "- Message of the day -");
        if (!st) return st;
        return send_numeric(376, nick_, "End of /MOTD command.");
    }
    return core::ok();
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
    auto st = send_numeric(321, nick_, "Channel :Users Name");
    if (!st) return st;
    // (stub: empty channel list)
    // 323 RPL_LISTEND
    return send_numeric(323, nick_, "End of /LIST");
}

core::Status<> WolFsm::on_join(std::string_view params) {
    if (state_ == WolState::Connecting || state_ == WolState::Authenticating) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }

    auto chan = trim(first_token(params));
    if (chan.empty()) {
        return send_numeric(461, nick_, "JOIN :Not enough parameters");
    }

    channel_ = std::string(chan);
    state_   = WolState::InChannel;

    // Echo JOIN back with user prefix.
    std::string join_echo = ":";
    join_echo += nick_;
    join_echo += " JOIN :";
    join_echo += channel_;
    auto st = send_raw(join_echo);
    if (!st) return st;

    // 332 RPL_TOPIC (empty topic for stub)
    st = send_numeric(332, channel_, "");
    if (!st) return st;

    // 366 RPL_ENDOFNAMES
    return send_numeric(366, channel_, "End of /NAMES list.");
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

    auto sp = params.find(' ');
    if (sp == std::string_view::npos) {
        return send_numeric(411, nick_, "No recipient given (PRIVMSG)");
    }

    // Stub: acknowledge but don't relay (Phase 5 will wire to post_message).
    return core::ok();
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
