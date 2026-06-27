// SPDX-License-Identifier: GPL-2.0-or-later
/// @file wol_fsm.cpp
/// WolFsm — thin coordinator.
///
/// This translation unit owns:
///   - Anonymous-namespace parsing helpers (parse_line, extract_trailing,
///     first_token, trim) — also exposed as named functions in
///     pvpgn::protocol::wol for use by sub-TUs via wol_fsm/wol_internal.hpp
///   - on_bytes()      — byte ingestion + line-buffer management
///   - on_close()      — session teardown
///   - process_lines() — CRLF/LF framing loop
///   - dispatch_line() — command routing
///   - send_numeric()  — numeric-reply builder
///   - send_raw()      — raw line sender
///
/// All command handler implementations live in the focused sub-TUs:
///   wol_fsm/wol_auth.cpp  — on_nick, on_user, on_pass, try_authenticate
///   wol_fsm/wol_chat.cpp  — on_ping, on_quit, on_list, on_join, on_part, on_privmsg

#include "protocol/wol/wol_fsm.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

#include "application/chat/leave_channel.hpp"
#include "application/game/wol_game_store.hpp"
#include "application/game/wol_user_flags_store.hpp"
#include "core/error.hpp"
#include "core/version.hpp"
#include "domain/connection/peer_address_store.hpp"
#include "domain/identity/ports.hpp"
#include "wol_fsm/wol_internal.hpp"

namespace pvpgn::protocol::wol {

// ===========================================================================
// Anonymous-namespace helpers (TU-local implementations)
// ===========================================================================

namespace {

// Maximum line length accepted from the client (IRC RFC 1459 limit).
constexpr std::size_t kMaxLineLen = 512;

/// ASCII upper-case a string in-place.
void to_upper(std::string& s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

}  // namespace

// ===========================================================================
// Named helpers — declared in wol_fsm/wol_internal.hpp, defined here
// ===========================================================================

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

std::string_view extract_trailing(std::string_view params) {
    auto colon = params.find(':');
    if (colon == std::string_view::npos) return {};
    return params.substr(colon + 1);
}

std::string_view first_token(std::string_view params) {
    auto sp = params.find(' ');
    if (sp == std::string_view::npos) return params;
    return params.substr(0, sp);
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.remove_suffix(1);
    return s;
}

// ===========================================================================
// WolFsm::on_bytes — byte ingestion + line-buffer management
// ===========================================================================

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

// ===========================================================================
// WolFsm::on_close
// ===========================================================================

void WolFsm::on_close() {
    state_ = WolState::Disconnecting;
    line_buf_.clear();
    // Release this session's account->session binding. try_wol_authenticate()
    // attached it on login; without the matching detach here the registry
    // accumulates a stale entry for every WOL account that ever connected
    // (and PostMessage would keep routing to a dead SessionId). Mirrors the
    // BNCS logout path. account_id_ stays 0 until authenticated, so the guard
    // also skips connections that closed before logging in.
    if (auth_.session_registry && account_id_.value() != 0) {
        auth_.session_registry->detach(session_id_);
    }
    if (peer_store_ && account_id_.value() != 0) {
        peer_store_->remove(account_id_);
    }
    if (user_flags_store_ && account_id_.value() != 0) {
        user_flags_store_->remove(account_id_);
    }
    // Leave the current channel + game so disconnecting clients don't ghost in
    // the roster / game (the BNCS path does this via on_disconnect; the WOL path
    // previously did not). Notify the remaining members with an IRC PART.
    if (account_id_.value() != 0 && channel_id_.value() != 0 && !channel_.empty()) {
        if (leave_channel_) {
            auto result = leave_channel_->execute(channel_id_, account_id_);
            if (result) {
                std::string line = ":";
                line += nick_;
                line += '!';
                line += nick_;
                line += "@Battle.net PART ";
                line += channel_;
                route_irc_line(line, result.value().members_to_notify);
            }
        }
        if (wol_game_store_) {
            std::string game_name = channel_;
            if (!game_name.empty() && game_name[0] == '#') game_name.erase(0, 1);
            wol_game_store_->remove_player(game_name, account_id_);
        }
    }
}

// ===========================================================================
// WolFsm::process_lines — CRLF/LF framing loop
// ===========================================================================

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

        // Per-line "excess flood" protection, mirroring the original server's
        // handle_irc_common_packet (handle_irc_common.cpp:336-345). The oracle
        // accumulates every non-'\n' byte of the line (so a CRLF terminator's
        // '\r' is counted too) and, once the running count exceeds
        // 100 + MAX_IRC_MESSAGE_LEN (= 612), logs "excess flood" and returns -1,
        // which the dispatch turns into conn_close_read() — the line is never
        // handled and the connection is destroyed, sending zero bytes back.
        // `consume - 1` is exactly that count (every consumed byte except the
        // terminating '\n'): line.size() for a bare-LF line, line.size()+1 for
        // CRLF. Mirror the oracle: drop the line, emit nothing, close the session.
        if (consume - 1 > kMaxLineLen + 100) {
            state_ = WolState::Disconnecting;
            ctx_->close();
            break;
        }

        // Below the flood cap but over the IRC line limit: the oracle stores the
        // command into ircline[MAX_IRC_MESSAGE_LEN] while ircpos < 511, so the
        // dispatched command is truncated to the first 511 chars. Match that.
        if (line.size() > kMaxLineLen - 1) {
            line.resize(kMaxLineLen - 1);
        }

        auto st = dispatch_line(line);
        if (!st) return st;

        if (state_ == WolState::Disconnecting) break;
    }
    return core::ok();
}

// ===========================================================================
// WolFsm::dispatch_line — command routing
// ===========================================================================

core::Status<> WolFsm::dispatch_line(std::string_view line) {
    auto [cmd, params] = parse_line(line);

    if (cmd.empty()) return core::ok();

    // Two-table login gate, mirroring the original server's con/log split
    // (handle_wol.cpp + handle_irc_common.cpp). A small set of "connected"
    // verbs are valid in any state; every other recognized verb requires a
    // completed login. handle_irc_common tries the con-table first, and if a
    // verb is not there AND the connection is not yet logged in, it emits
    // exactly one "421 ... :Unrecognized command (before login)". (The 451
    // ERR_NOTREGISTERED numeric is defined by the original but never sent, so
    // v3 must not emit it either.)
    static constexpr std::string_view con_verbs[] = {
        "NICK", "USER", "PASS", "PING", "PONG", "QUIT", "PRIVMSG", "CVERS",
        "VERCHK", "APGAR", "SETOPT", "SERIAL", "LISTSEARCH", "RUNGSEARCH",
        "HIGHSCORE",
    };
    bool is_con_verb = false;
    for (auto v : con_verbs) {
        if (cmd == v) { is_con_verb = true; break; }
    }
    const bool logged_in = !(state_ == WolState::Connecting ||
                             state_ == WolState::Authenticating);
    if (!is_con_verb && !logged_in) {
        return send_numeric(421, nick_.empty() ? "*" : nick_,
                            "Unrecognized command (before login)");
    }

    if (cmd == "NICK")    return on_nick(params);
    if (cmd == "USER")    return on_user(params);
    if (cmd == "PASS")    return on_pass(params);
    if (cmd == "CVERS")   return on_cvers(params);
    if (cmd == "VERCHK")  return on_verchk(params);
    if (cmd == "APGAR")   return on_apgar(params);
    if (cmd == "PING")    return on_ping(params);
    if (cmd == "PONG")    return core::ok();  // ignore client PONGs
    if (cmd == "QUIT")    return on_quit(params);
    if (cmd == "LIST")    return on_list(params);
    if (cmd == "JOIN")    return on_join(params);
    if (cmd == "PART")    return on_part(params);
    if (cmd == "NAMES")   return on_names(params);
    if (cmd == "TIME")    return on_time();
    if (cmd == "MODE")    return on_mode(params);
    if (cmd == "KICK")    return on_kick(params);
    if (cmd == "TOPIC")   return on_topic(params);
    if (cmd == "PRIVMSG") return on_privmsg(params);
    if (cmd == "GAMEOPT") return on_gameopt(params);
    if (cmd == "JOINGAME") return on_joingame(params);
    if (cmd == "FINDUSER") return on_finduser(params, /*ex=*/false);
    if (cmd == "FINDUSEREX") return on_finduser(params, /*ex=*/true);
    if (cmd == "GETBUDDY") return on_getbuddy();
    if (cmd == "ADDBUDDY") return on_addbuddy(params);
    if (cmd == "DELBUDDY") return on_delbuddy(params);
    if (cmd == "SETCODEPAGE") return on_setcodepage(params);
    if (cmd == "GETCODEPAGE") return on_getcodepage(params);
    if (cmd == "SETLOCALE") return on_setlocale(params);
    if (cmd == "GETLOCALE") return on_getlocale(params);
    if (cmd == "GETINSIDER") return on_getinsider(params);
    if (cmd == "PAGE") return on_page(params);
    if (cmd == "CHANCHK") return on_chanchk(params);
    if (cmd == "HOST") return on_host(params);
    if (cmd == "USERIP") return on_userip(params);
    if (cmd == "INVMSG") return on_invmsg(params);
    if (cmd == "HIGHSCORE") return on_highscore(params);
    if (cmd == "LISTSEARCH") return on_listsearch(params);
    if (cmd == "RUNGSEARCH") return on_rungsearch(params);
    if (cmd == "STARTG") return on_startg(params);
    if (cmd == "SETOPT") return on_setopt(params);
    if (cmd == "ADVERTR") return on_advertr(params);
    if (cmd == "SQUADINFO" || cmd == "CLANBYNAME") {
        // The original (_handle_squadinfo_command / _handle_clanbyname_command)
        // requires a parameter — 461 ERR_NEEDMOREPARAMS when missing — and then
        // looks up the clan, which a freshly-created account never has, yielding
        // 439 ERR_IDNOEXIST. v3 has no clan backend, so the lookup path always
        // reports "no clan". (Previously these were silent no-ops in wol_known[].)
        if (first_token(params).empty()) {
            return send_needmoreparams(cmd);
        }
        return send_numeric(439, nick_.empty() ? "*" : nick_,
                            std::string(cmd) + " :ID does not exist");
    }

    // COPYRIGHT / WARRANTY / LICENSE / VERSION: the original routes these through
    // its chat-command handler (_handle_copyright_command / _handle_version_command),
    // which for a WOL connection renders each output line as a PAGE. v3 had let
    // them fall through to "Unknown command.". Emit the same GPL block / version.
    if (cmd == "COPYRIGHT" || cmd == "WARRANTY" || cmd == "LICENSE") {
        static constexpr const char* kCopyright[] = {
            " Copyright (C) 2002 - 2014  See source for details",
            " ",
            " PvPGN is free software; you can redistribute it and/or",
            " modify it under the terms of the GNU General Public License",
            " as published by the Free Software Foundation; either version 2",
            " of the License, or (at your option) any later version.",
            " ",
            " This program is distributed in the hope that it will be useful,",
            " but WITHOUT ANY WARRANTY; without even the implied warranty of",
            " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
            " GNU General Public License for more details.",
            " ",
            " You should have received a copy of the GNU General Public License",
            " along with this program; if not, write to the Free Software",
            " Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.",
        };
        const std::string who = nick_.empty() ? "*" : nick_;
        for (const char* cline : kCopyright) {
            std::string p = ":";
            p += ctx_->server_name();
            p += " PAGE ";
            p += who;
            p += " :";
            p += cline;
            if (auto s = send_raw(p); !s) return s;
        }
        return core::ok();
    }
    if (cmd == "VERSION") {
        std::string p = ":";
        p += ctx_->server_name();
        p += " PAGE ";
        p += nick_.empty() ? "*" : nick_;
        p += " :PvPGN ";
        p += core::kVersionString;
        return send_raw(p);
    }

    // WOL-specific commands that we acknowledge but don't fully implement yet.
    // CVERS, VERCHK, APGAR, SERIAL, etc. (ADVERTC is a no-op in the original too.)
    // Return 421 ERR_UNKNOWNCOMMAND for truly unknown commands.
    const std::string_view wol_known[] = {
        "SERIAL",
        "ADVERTC",
        "INVDEL",
    };
    for (auto kw : wol_known) {
        if (cmd == kw) return core::ok();  // silently accept
    }

    // Unknown command, logged in. The original routes the unknown verb to its
    // bnet chat-command handler, which fails with message_type_error
    // "Unknown command." That error, for a WOL connection, is rendered as a
    // PAGE line (irc.cpp), NOT a 421 numeric. Mirror that so a logged-in WOL
    // client gets the same feedback as the oracle. (The pre-login case is
    // handled earlier by the two-table login gate, which already emitted 421
    // for non-con verbs.)
    std::string page = ":";
    page += ctx_->server_name();
    page += " PAGE ";
    page += nick_.empty() ? "*" : nick_;
    page += " :Unknown command.";
    return send_raw(page);
}

// ===========================================================================
// Reply helpers
// ===========================================================================

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

core::Status<> WolFsm::send_needmoreparams(std::string_view cmd) {
    // Wire form: ":<server> 461 <nick> <CMD> :Not enough parameters".
    // The command name is a middle parameter, so it goes into the `target`
    // field (send_numeric injects the ':' only before the trailing text).
    std::string target = nick_.empty() ? std::string("*") : nick_;
    target += ' ';
    target += cmd;
    return send_numeric(461, target, "Not enough parameters");
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
