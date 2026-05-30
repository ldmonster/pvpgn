// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_game_lifecycle.hpp
/// Game-lifecycle and misc/anti-cheat messages: CloseGame, StartGame, JoinGame, GameReport, ReadMemory, MapAuth.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages/messages_common.hpp"

namespace pvpgn::protocol::bnet {

struct CloseGame {
    bool operator==(const CloseGame&) const = default;
};
struct CloseGame2 {
    bool operator==(const CloseGame2&) const = default;
};

/// 0x08 CLIENT_STARTGAME1 (original StarCraft / shareware).
struct StartGame1Request {
    std::uint32_t status   = 0;
    std::uint32_t unknown3 = 0;
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::string   game_name;
    std::string   password;
    std::string   info;
    bool operator==(const StartGame1Request&) const = default;
};

/// 0x08 SERVER_STARTGAME1_ACK. `reply` = 0 on success.
struct StartGame1Ack {
    std::uint32_t reply = 0;
    bool operator==(const StartGame1Ack&) const = default;
};

/// 0x1A CLIENT_STARTGAME3 (StarCraft 1.03 / Diablo 1.07).
struct StartGame3Request {
    std::uint32_t status   = 0;
    std::uint32_t unknown3 = 0;
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint32_t unknown6 = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::string   game_name;
    std::string   password;
    std::string   info;
    bool operator==(const StartGame3Request&) const = default;
};

/// 0x1A SERVER_STARTGAME3_ACK. `reply` = 0 on success.
struct StartGame3Ack {
    std::uint32_t reply = 0;
    bool operator==(const StartGame3Ack&) const = default;
};

/// 0x22 CLIENT_JOIN_GAME: client tells server it joined game `game_name`.
struct JoinGame {
    std::uint32_t clienttag  = 0;
    std::uint32_t versiontag = 0;
    std::string   game_name;
    std::string   password;
    bool operator==(const JoinGame&) const = default;
};

/// 0x2C CLIENT_GAME_REPORT: per-player results + free-form report blob.
/// Wire layout: u32 unknown1, u32 count, count × u32 result, count × cstring
/// player name, cstring report_header, cstring report_body.
struct GameReport {
    std::uint32_t              unknown1 = 0;
    std::vector<std::uint32_t> results;       ///< one result per player slot
    std::vector<std::string>   player_names;  ///< parallel array, same size
    std::string                report_header;
    std::string                report_body;
    bool operator==(const GameReport&) const = default;
};
inline constexpr std::uint32_t kGameReportResultPlaying    = 0;
inline constexpr std::uint32_t kGameReportResultWin        = 1;
inline constexpr std::uint32_t kGameReportResultLoss       = 2;
inline constexpr std::uint32_t kGameReportResultDraw       = 3;
inline constexpr std::uint32_t kGameReportResultDisconnect = 4;
inline constexpr std::uint32_t kGameReportResultObserver   = 5;

// --- Misc / anti-cheat / advisory ---------------------------------------

/// 0x17 server → client: anti-cheat "read N bytes at address" request.
struct ReadMemoryRequest {
    std::uint32_t request_id = 0;
    std::uint32_t address    = 0;
    std::uint32_t length     = 0;
    bool operator==(const ReadMemoryRequest&) const = default;
};

/// 0x17 client → server: anti-cheat memory contents reply.
struct ReadMemoryReply {
    std::uint32_t             request_id = 0;
    std::vector<std::uint8_t> memory;
    bool operator==(const ReadMemoryReply&) const = default;
};

/// 0x1B client → server: post-join game UDP/IP advisory.
/// `port` and `ip` are stored on the wire in big-endian byte order; here
/// we keep them as raw u32/u16 fields to preserve byte-exact round-trip.
struct Unknown1B {
    std::uint16_t unknown1 = 0;
    std::uint16_t port_be  = 0;  ///< big-endian on wire; preserved verbatim
    std::uint32_t ip_be    = 0;  ///< big-endian on wire; preserved verbatim
    std::uint32_t unknown2 = 0;
    std::uint32_t unknown3 = 0;
    bool operator==(const Unknown1B&) const = default;
};

/// 0x24 client → server: empty advisory packet (purpose unknown).
struct Unknown24 {
    bool operator==(const Unknown24&) const = default;
};

/// 0x32 client → server: map-checksum auth request.
struct MapAuthReq1 {
    std::array<std::uint32_t, 5> file_checksum{};
    std::string                  mapfile;
    bool operator==(const MapAuthReq1&) const = default;
};

/// 0x32 server → client.
struct MapAuthReply1 {
    std::uint32_t response = 0;
    bool operator==(const MapAuthReply1&) const = default;
};
inline constexpr std::uint32_t kMapAuthReply1ResponseNo       = 0;
inline constexpr std::uint32_t kMapAuthReply1ResponseOk       = 1;
inline constexpr std::uint32_t kMapAuthReply1ResponseLadderOk = 2;

/// 0x3C client → server: map-checksum auth v2.
struct MapAuthReq2 {
    std::uint32_t                unknown = 0;
    std::array<std::uint32_t, 5> file_hash{};
    std::string                  mapfile;
    bool operator==(const MapAuthReq2&) const = default;
};

/// 0x3C server → client.
struct MapAuthReply2 {
    std::uint32_t response = 0;
    bool operator==(const MapAuthReply2&) const = default;
};

/// 0x5C client → server: switch active client tag (cross-game session).
struct ChangeClient {
    std::uint32_t clienttag = 0;
    bool operator==(const ChangeClient&) const = default;
};

// Message variants. Add new arms as new SIDs are wired up.

}  // namespace pvpgn::protocol::bnet
