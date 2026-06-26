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
            return send_numeric(461, nick_.empty() ? "*" : nick_,
                                std::string(cmd) + " :Not enough parameters");
        }
        return send_numeric(439, nick_.empty() ? "*" : nick_,
                            std::string(cmd) + " :ID does not exist");
    }

    // WOL-specific commands that we acknowledge but don't fully implement yet.
    // CVERS, VERCHK, APGAR, SERIAL, etc. (ADVERTC is a no-op in the original too.)
    // Return 421 ERR_UNKNOWNCOMMAND for truly unknown commands.
    const std::string_view wol_known[] = {
        "SERIAL",
        "ADVERTC",
        "INVDEL",
        "LISTSEARCH", "RUNGSEARCH", "HIGHSCORE",
        "TOPIC", "TIME", "KICK", "MODE",
    };
    for (auto kw : wol_known) {
        if (cmd == kw) return core::ok();  // silently accept
    }

    // 421 ERR_UNKNOWNCOMMAND
    return send_numeric(421, nick_.empty() ? "*" : nick_,
                        cmd + " :Unknown command");
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
