// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_game.hpp
/// Game messages: GameList, LadderSearch, FileInfo.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages/messages_common.hpp"

namespace pvpgn::protocol::bnet {

struct GameListRequest {
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
    std::uint32_t unknown3 = 0;
    std::uint32_t max_games = 0;
    std::string   game_name;   ///< empty = "any game"
    bool operator==(const GameListRequest&) const = default;
};

/// Entry inside a SERVER_GAMELISTREPLY. Ports + IPs are kept in their
/// wire byte order (big-endian on the wire); the codec stores them as
/// host integers reconstructed from the BE bytes.
struct GameListEntry {
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint16_t unknown3 = 0;
    std::uint16_t port     = 0;   ///< host order (wire was BE)
    std::uint32_t game_ip  = 0;   ///< host order (wire was BE)
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::uint32_t status   = 0;
    std::uint32_t unknown6 = 0;
    std::string   game_name;
    std::string   password;       ///< clear-text legacy field; empty if none
    std::string   info;
    bool operator==(const GameListEntry&) const = default;
};

/// SID_GETADVLISTEX (server → client).
/// When `sstatus` ≠ 0 the entries vector is empty and the status code
/// carries the per-request error.
struct GameListReply {
    std::uint32_t                sstatus = 0;
    std::vector<GameListEntry>   entries;
    bool operator==(const GameListReply&) const = default;
};

// --- SID_LADDERSEARCH (0x2F) — find player on ladder -----------------------

struct LadderSearchRequest {
    std::uint32_t client_tag = 0;   ///< e.g. 'SEXP', 'W3XP'
    std::uint32_t id         = 0;   ///< 1 standard, 3 ironman
    std::uint32_t type       = 0;   ///< 0 rated, 2 wins, 3 games
    std::string   player_name;
    bool operator==(const LadderSearchRequest&) const = default;
};

struct LadderSearchReply {
    std::uint32_t rank = 0xFFFFFFFFu;   ///< 0 = first; 0xFFFFFFFF = none
    bool operator==(const LadderSearchReply&) const = default;
};

// --- SID_GETFILETIME (0x33) — file-transfer init ---------------------------

struct FileInfoRequest {
    std::uint32_t type     = 0;   ///< TOS=0x1A, gateways=0x1B, ...
    std::uint32_t unknown2 = 0;   ///< always zero on the wire
    std::string   filename;
    bool operator==(const FileInfoRequest&) const = default;
};

struct FileInfoReply {
    std::uint32_t type      = 0;
    std::uint32_t unknown2  = 0;
    std::uint64_t timestamp = 0;  ///< Windows FILETIME (100-ns since 1601)
    std::string   filename;
    bool operator==(const FileInfoReply&) const = default;
};

}  // namespace pvpgn::protocol::bnet
