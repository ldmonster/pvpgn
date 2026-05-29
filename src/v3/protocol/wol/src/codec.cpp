// SPDX-License-Identifier: GPL-2.0-or-later

/// @file codec.cpp
/// WOL (Westwood Online) IRC-like text protocol codec implementation.
///
/// The codec operates at the *line* level. The FSM accumulates raw bytes,
/// splits on \r\n, and passes each complete line to `decode_client()`.
/// Encoding produces CRLF-terminated strings ready for the wire.

#include "protocol/wol/codec.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>

namespace pvpgn::protocol::wol {

namespace {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// ASCII upper-case a string_view into a new std::string.
std::string to_upper(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

/// Trim leading and trailing ASCII whitespace from a string_view.
std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

/// Return the first space-delimited token from `s` and advance `s` past it
/// (and past any following spaces).
std::string_view next_token(std::string_view& s) noexcept {
    // Skip leading spaces.
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    if (s.empty()) return {};
    const auto sp = s.find(' ');
    if (sp == std::string_view::npos) {
        auto tok = s;
        s = {};
        return tok;
    }
    auto tok = s.substr(0, sp);
    s.remove_prefix(sp + 1);
    return tok;
}

/// Extract the IRC trailing parameter (everything after the first ':').
/// Returns empty if no ':' is found.
std::string_view extract_trailing(std::string_view params) noexcept {
    const auto colon = params.find(':');
    if (colon == std::string_view::npos) return {};
    return params.substr(colon + 1);
}

/// Convenience: return a DecodeError failure.
core::Failure<common::DecodeError> decode_err(common::DecodeError e) {
    return core::fail(e);
}

// ---------------------------------------------------------------------------
// Per-command decoders
// ---------------------------------------------------------------------------

/// NICK <nickname>
core::Result<ClientMessage, common::DecodeError>
decode_nick(std::string_view params) {
    auto nick = trim(params);
    if (nick.empty())
        return decode_err(common::DecodeError::MalformedString);
    // Strip leading ':' if present (some clients send "NICK :foo").
    if (!nick.empty() && nick.front() == ':') nick.remove_prefix(1);
    if (nick.empty())
        return decode_err(common::DecodeError::MalformedString);
    return ClientMessage{Nick{std::string(nick)}};
}

/// USER <username> <hostname> <servername> :<realname>
core::Result<ClientMessage, common::DecodeError>
decode_user(std::string_view params) {
    std::string_view rest = params;
    auto username   = next_token(rest);
    auto hostname   = next_token(rest);
    auto servername = next_token(rest);
    if (username.empty())
        return decode_err(common::DecodeError::MalformedString);
    // Realname is the trailing param (after ':').
    auto realname = extract_trailing(rest);
    if (realname.empty()) realname = username;  // fallback
    return ClientMessage{User{
        std::string(username),
        std::string(hostname),
        std::string(servername),
        std::string(realname),
    }};
}

/// PASS <password>
core::Result<ClientMessage, common::DecodeError>
decode_pass(std::string_view params) {
    auto pw = trim(params);
    if (!pw.empty() && pw.front() == ':') pw.remove_prefix(1);
    // Empty password is technically valid (some clients send "PASS :")
    return ClientMessage{Pass{std::string(pw)}};
}

/// PING <token>
core::Result<ClientMessage, common::DecodeError>
decode_ping(std::string_view params) {
    auto token = trim(params);
    if (!token.empty() && token.front() == ':') token.remove_prefix(1);
    return ClientMessage{Ping{std::string(token)}};
}

/// PONG :<token>
core::Result<ClientMessage, common::DecodeError>
decode_pong(std::string_view params) {
    auto token = trim(params);
    if (!token.empty() && token.front() == ':') token.remove_prefix(1);
    return ClientMessage{Pong{std::string(token)}};
}

/// QUIT [:<reason>]
core::Result<ClientMessage, common::DecodeError>
decode_quit(std::string_view params) {
    auto reason = trim(params);
    if (!reason.empty() && reason.front() == ':') reason.remove_prefix(1);
    return ClientMessage{Quit{std::string(reason)}};
}

/// LIST
core::Result<ClientMessage, common::DecodeError>
decode_list(std::string_view /*params*/) {
    return ClientMessage{List{}};
}

/// JOIN #<channel>
core::Result<ClientMessage, common::DecodeError>
decode_join(std::string_view params) {
    std::string_view rest = params;
    auto chan = next_token(rest);
    if (chan.empty())
        return decode_err(common::DecodeError::MalformedString);
    return ClientMessage{Join{std::string(chan)}};
}

/// PART #<channel> [:<reason>]
core::Result<ClientMessage, common::DecodeError>
decode_part(std::string_view params) {
    std::string_view rest = params;
    auto chan = next_token(rest);
    if (chan.empty())
        return decode_err(common::DecodeError::MalformedString);
    auto reason = extract_trailing(rest);
    return ClientMessage{Part{std::string(chan), std::string(reason)}};
}

/// PRIVMSG <target> :<message>
core::Result<ClientMessage, common::DecodeError>
decode_privmsg(std::string_view params) {
    std::string_view rest = params;
    auto target = next_token(rest);
    if (target.empty())
        return decode_err(common::DecodeError::MalformedString);
    auto message = extract_trailing(rest);
    return ClientMessage{Privmsg{std::string(target), std::string(message)}};
}

/// CVERS <client_id> <version>
core::Result<ClientMessage, common::DecodeError>
decode_cvers(std::string_view params) {
    std::string_view rest = params;
    auto client_id = next_token(rest);
    auto version   = trim(rest);
    return ClientMessage{Cvers{std::string(client_id), std::string(version)}};
}

/// VERCHK <client_id> <version>
core::Result<ClientMessage, common::DecodeError>
decode_verchk(std::string_view params) {
    std::string_view rest = params;
    auto client_id = next_token(rest);
    auto version   = trim(rest);
    return ClientMessage{Verchk{std::string(client_id), std::string(version)}};
}

/// APGAR <password_hash> <flags>
core::Result<ClientMessage, common::DecodeError>
decode_apgar(std::string_view params) {
    std::string_view rest = params;
    auto hash  = next_token(rest);
    auto flags = trim(rest);
    return ClientMessage{Apgar{std::string(hash), std::string(flags)}};
}

/// SETOPT <option> <value>
core::Result<ClientMessage, common::DecodeError>
decode_setopt(std::string_view params) {
    std::string_view rest = params;
    auto option = next_token(rest);
    auto value  = trim(rest);
    return ClientMessage{Setopt{std::string(option), std::string(value)}};
}

/// SERIAL <serial_number>
core::Result<ClientMessage, common::DecodeError>
decode_serial(std::string_view params) {
    auto serial = trim(params);
    return ClientMessage{Serial{std::string(serial)}};
}

/// GAMEOPT <options>
core::Result<ClientMessage, common::DecodeError>
decode_gameopt(std::string_view params) {
    return ClientMessage{Gameopt{std::string(trim(params))}};
}

/// STARTG <game_name> [<player>...]
core::Result<ClientMessage, common::DecodeError>
decode_startg(std::string_view params) {
    std::string_view rest = params;
    auto game_name = next_token(rest);
    if (game_name.empty())
        return decode_err(common::DecodeError::MalformedString);
    Startg msg;
    msg.game_name = std::string(game_name);
    // Remaining tokens are player nicks.
    while (true) {
        auto player = next_token(rest);
        if (player.empty()) break;
        msg.players.emplace_back(player);
    }
    return ClientMessage{std::move(msg)};
}

/// JOINGAME <game_name> <host_ip> <host_port>
core::Result<ClientMessage, common::DecodeError>
decode_joingame(std::string_view params) {
    std::string_view rest = params;
    auto game_name  = next_token(rest);
    auto host_ip    = next_token(rest);
    auto host_port  = trim(rest);
    if (game_name.empty())
        return decode_err(common::DecodeError::MalformedString);
    return ClientMessage{Joingame{
        std::string(game_name),
        std::string(host_ip),
        std::string(host_port),
    }};
}

/// FINDUSER <nickname>
core::Result<ClientMessage, common::DecodeError>
decode_finduser(std::string_view params) {
    auto nick = trim(params);
    if (nick.empty())
        return decode_err(common::DecodeError::MalformedString);
    return ClientMessage{Finduser{std::string(nick)}};
}

/// PAGE <nickname> :<message>
core::Result<ClientMessage, common::DecodeError>
decode_page(std::string_view params) {
    std::string_view rest = params;
    auto nick    = next_token(rest);
    auto message = extract_trailing(rest);
    if (nick.empty())
        return decode_err(common::DecodeError::MalformedString);
    return ClientMessage{Page{std::string(nick), std::string(message)}};
}

/// ADDBUDDY <nickname>
core::Result<ClientMessage, common::DecodeError>
decode_addbuddy(std::string_view params) {
    auto nick = trim(params);
    if (nick.empty())
        return decode_err(common::DecodeError::MalformedString);
    return ClientMessage{Addbuddy{std::string(nick)}};
}

/// DELBUDDY <nickname>
core::Result<ClientMessage, common::DecodeError>
decode_delbuddy(std::string_view params) {
    auto nick = trim(params);
    if (nick.empty())
        return decode_err(common::DecodeError::MalformedString);
    return ClientMessage{Delbuddy{std::string(nick)}};
}

/// GETBUDDY
core::Result<ClientMessage, common::DecodeError>
decode_getbuddy(std::string_view /*params*/) {
    return ClientMessage{Getbuddy{}};
}

/// Generic WOL-specific command (acknowledged but not fully structured).
core::Result<ClientMessage, common::DecodeError>
decode_wol_command(std::string_view command, std::string_view params) {
    return ClientMessage{WolCommand{std::string(command), std::string(params)}};
}

// ---------------------------------------------------------------------------
// Known WOL-specific commands that map to WolCommand (catch-all)
// ---------------------------------------------------------------------------

constexpr std::string_view kWolKnown[] = {
    "ADVERTR", "ADVERTC", "CHANCHK", "HOST", "INVMSG", "INVDEL",
    "USERIP", "SQUADINFO", "CLANBYNAME", "SETCODEPAGE", "GETCODEPAGE",
    "SETLOCALE", "GETLOCALE", "GETINSIDER", "LISTSEARCH", "RUNGSEARCH",
    "HIGHSCORE", "NAMES", "TOPIC", "TIME", "KICK", "MODE", "FINDUSEREX",
};

bool is_wol_known(std::string_view cmd) noexcept {
    for (auto kw : kWolKnown) {
        if (cmd == kw) return true;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// decode_client
// ---------------------------------------------------------------------------

core::Result<ClientMessage, common::DecodeError>
decode_client(std::string_view line) {
    if (line.empty())
        return core::fail(common::DecodeError::Truncated);

    // Skip optional IRC prefix ":something ".
    std::size_t pos = 0;
    if (line[0] == ':') {
        const auto sp = line.find(' ');
        if (sp == std::string_view::npos)
            return core::fail(common::DecodeError::MalformedString);
        pos = sp + 1;
        while (pos < line.size() && line[pos] == ' ') ++pos;
    }

    // Extract command (up to next space or end-of-line).
    const auto cmd_start = pos;
    while (pos < line.size() && line[pos] != ' ') ++pos;
    if (pos == cmd_start)
        return core::fail(common::DecodeError::MalformedString);

    const auto cmd_raw = line.substr(cmd_start, pos - cmd_start);
    const auto cmd     = to_upper(cmd_raw);

    // Skip space between command and params.
    if (pos < line.size() && line[pos] == ' ') ++pos;
    const auto params = line.substr(pos);

    // Dispatch to per-command decoders.
    if (cmd == kCmdNick)     return decode_nick(params);
    if (cmd == kCmdUser)     return decode_user(params);
    if (cmd == kCmdPass)     return decode_pass(params);
    if (cmd == kCmdPing)     return decode_ping(params);
    if (cmd == kCmdPong)     return decode_pong(params);
    if (cmd == kCmdQuit)     return decode_quit(params);
    if (cmd == kCmdList)     return decode_list(params);
    if (cmd == kCmdJoin)     return decode_join(params);
    if (cmd == kCmdPart)     return decode_part(params);
    if (cmd == kCmdPrivmsg)  return decode_privmsg(params);
    if (cmd == kCmdCvers)    return decode_cvers(params);
    if (cmd == kCmdVerchk)   return decode_verchk(params);
    if (cmd == kCmdApgar)    return decode_apgar(params);
    if (cmd == kCmdSetopt)   return decode_setopt(params);
    if (cmd == kCmdSerial)   return decode_serial(params);
    if (cmd == kCmdGameopt)  return decode_gameopt(params);
    if (cmd == kCmdStartg)   return decode_startg(params);
    if (cmd == kCmdJoingame) return decode_joingame(params);
    if (cmd == kCmdFinduser) return decode_finduser(params);
    if (cmd == kCmdPage)     return decode_page(params);
    if (cmd == kCmdAddbuddy) return decode_addbuddy(params);
    if (cmd == kCmdDelbuddy) return decode_delbuddy(params);
    if (cmd == kCmdGetbuddy) return decode_getbuddy(params);

    // Known WOL-specific commands that we accept but don't fully structure.
    if (is_wol_known(cmd))
        return decode_wol_command(cmd, params);

    return core::fail(common::DecodeError::UnknownOpcode);
}

// ---------------------------------------------------------------------------
// encode_server
// ---------------------------------------------------------------------------

std::string encode_server(const NumericReply& msg) {
    // Format: ":server_name NNN target :text\r\n"
    char code_str[4];
    code_str[0] = static_cast<char>('0' + (msg.code / 100) % 10);
    code_str[1] = static_cast<char>('0' + (msg.code / 10)  % 10);
    code_str[2] = static_cast<char>('0' + msg.code % 10);
    code_str[3] = '\0';

    std::string out;
    out.reserve(msg.server_name.size() + msg.target.size() + msg.text.size() + 16);
    out.push_back(':');
    out.append(msg.server_name);
    out.push_back(' ');
    out.append(code_str, 3);
    out.push_back(' ');
    out.append(msg.target);
    out.append(" :");
    out.append(msg.text);
    out.append("\r\n");
    return out;
}

std::string encode_server(const RawLine& msg) {
    std::string out;
    out.reserve(msg.line.size() + 2);
    out.append(msg.line);
    out.append("\r\n");
    return out;
}

std::string encode_server(const ServerMessage& msg) {
    return std::visit(
        [](const auto& m) { return encode_server(m); },
        msg);
}

}  // namespace pvpgn::protocol::wol
