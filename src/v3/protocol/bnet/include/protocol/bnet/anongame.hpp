// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file anongame.hpp
/// Typed sub-message layer for SID_WARCRAFTGENERAL / FINDANONGAME (0x44).
///
/// The wire codec in `codec.hpp` transports 0x44 packets as opaque
/// `WarcraftGeneralRequest{sub_option, data}` /
/// `WarcraftGeneralReply{sub_option, data}` envelopes. This header adds a
/// secondary, application-layer parsing pass over the `data` field, mapping
/// each known sub-option (SEARCH / FOUND / CANCEL / INFOREQ / PROFILE /
/// TOURNAMENT / AT_SEARCH / AT_INVITER_SEARCH / PROFILE_CLAN / GET_ICON /
/// SET_ICON) to a strongly typed struct and a discriminated variant.
///
/// Layouts come verbatim from legacy `src/common/anongame_protocol.h`.
///
/// Lifecycle:
///   wire bytes
///     ⟶ decode_client/decode_server → WarcraftGeneralRequest/Reply
///         ⟶ parse_findanongame_request/reply → AnonGameClient/Server
///           (variant of typed sub-messages)
///   and inverse via serialize_findanongame_*.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"

namespace pvpgn::protocol::bnet {

// --- 0x00 client: SEARCH (PG matchmaking) --------------------------------
struct AnonGameSearch {
    std::uint32_t count     = 0;
    std::uint32_t unknown2  = 0;
    std::uint8_t  type      = 0;  // 0=PG 1=AT 2=TY
    std::uint8_t  gametype  = 0;  // 0=1v1 1=2v2 2=3v3 3=4v4 4=sffa
    std::uint32_t map_prefs = 0;
    std::uint8_t  unknown3  = 0;
    std::uint32_t id        = 0;
    std::uint32_t race      = 0;
    bool operator==(const AnonGameSearch&) const = default;
};

// --- 0x05 client: AT_SEARCH ----------------------------------------------
struct AnonGameAtSearch {
    std::uint32_t                 count     = 0;
    std::uint32_t                 tid       = 0;
    std::uint32_t                 timestamp = 0;
    std::uint8_t                  teamsize  = 0;
    std::array<std::uint32_t, 5>  info{};
    std::uint32_t                 unknown2  = 0;
    std::uint8_t                  unknown3  = 0;
    std::uint32_t                 id        = 0;
    std::uint32_t                 race      = 0;
    bool operator==(const AnonGameAtSearch&) const = default;
};

// --- 0x06 client: AT_INVITER_SEARCH --------------------------------------
struct AnonGameAtInviterSearch {
    std::uint32_t                 count     = 0;
    std::uint32_t                 tid       = 0;
    std::uint32_t                 timestamp = 0;
    std::uint8_t                  teamsize  = 0;
    std::array<std::uint32_t, 5>  info{};
    std::uint32_t                 unknown2  = 0;
    std::uint8_t                  type      = 0;
    std::uint8_t                  gametype  = 0;
    std::uint32_t                 map_prefs = 0;
    std::uint8_t                  unknown3  = 0;
    std::uint32_t                 id        = 0;
    std::uint32_t                 race      = 0;
    bool operator==(const AnonGameAtInviterSearch&) const = default;
};

// --- 0x02 client: INFOREQ -------------------------------------------------
// Well-known info tags carried in `AnonGameInfoRequestEntry::tag`.
// On the wire the bytes are stored verbatim (LE u32); the constants below
// are the resulting integer values. Server reply uses the reversed-string
// tags (see `kAnonGameInfoTagServer*`).
inline constexpr std::uint32_t kAnonGameInfoTagURL    = 0x004C5255u;  // 'URL\0'
inline constexpr std::uint32_t kAnonGameInfoTagMAP    = 0x0050414Du;  // 'MAP\0'
inline constexpr std::uint32_t kAnonGameInfoTagTYPE   = 0x45505954u;  // 'TYPE'
inline constexpr std::uint32_t kAnonGameInfoTagDESC   = 0x43534544u;  // 'DESC'
inline constexpr std::uint32_t kAnonGameInfoTagLADR   = 0x5244414Cu;  // 'LADR'
// Server-side acknowledgements use the same four ASCII bytes reversed.
inline constexpr std::uint32_t kAnonGameInfoTagServerURL  = 0x0055524Cu;  // 'LRU\0'
inline constexpr std::uint32_t kAnonGameInfoTagServerMAP  = 0x004D4150u;  // 'PAM\0'
inline constexpr std::uint32_t kAnonGameInfoTagServerTYPE = 0x54595045u;  // 'EPYT'
inline constexpr std::uint32_t kAnonGameInfoTagServerDESC = 0x44455343u;  // 'CSED'
inline constexpr std::uint32_t kAnonGameInfoTagServerLADR = 0x4C414452u;  // 'RDAL'

struct AnonGameInfoRequestEntry {
    std::uint32_t tag     = 0;
    std::uint32_t tag_unk = 0;
    bool operator==(const AnonGameInfoRequestEntry&) const = default;
};

struct AnonGameInfoRequest {
    std::uint32_t                            count   = 0;
    std::uint8_t                             noitems = 0;
    std::vector<AnonGameInfoRequestEntry>    entries;
    bool operator==(const AnonGameInfoRequest&) const = default;
};

// --- 0x03 client: cancel (count only) ------------------------------------
struct AnonGameClientCancel {
    std::uint32_t count = 0;
    bool operator==(const AnonGameClientCancel&) const = default;
};

// --- 0x04 client: profile -------------------------------------------------
struct AnonGameProfileRequest {
    std::uint32_t count      = 0;
    std::string   username;
    std::string   client_tag;
    bool operator==(const AnonGameProfileRequest&) const = default;
};

// --- 0x07 client: tournament request -------------------------------------
struct AnonGameTournamentRequest {
    std::uint32_t count = 0;
    bool operator==(const AnonGameTournamentRequest&) const = default;
};

// --- 0x08 client: clan profile request -----------------------------------
struct AnonGameClanProfileRequest {
    std::uint32_t count    = 0;
    std::uint32_t clan_tag = 0;
    bool operator==(const AnonGameClanProfileRequest&) const = default;
};

// --- 0x09 client: GET_ICON -----------------------------------------------
struct AnonGameGetIcon {
    std::uint32_t count = 0;
    bool operator==(const AnonGameGetIcon&) const = default;
};

// --- 0x0A client: SET_ICON -----------------------------------------------
struct AnonGameSetIcon {
    std::uint32_t count = 0;
    std::uint32_t icon  = 0;
    bool operator==(const AnonGameSetIcon&) const = default;
};

// --- 0x00 server: SEARCH reply -------------------------------------------
struct AnonGameSearchReply {
    std::uint32_t count = 0;
    std::uint32_t reply = 0;
    bool operator==(const AnonGameSearchReply&) const = default;
};

// --- 0x01 server: FOUND ---------------------------------------------------
// `SafPt2` is the fixed 14-byte trailer (legacy `t_saf_pt2`) that follows
// the NUL-terminated mapname. Any bytes after it are preserved in
// `extras` so we round-trip byte-exact even if a future client emits more.
struct SafPt2 {
    std::uint32_t unknown1        = 0;  // typically 0xFFFFFFFF
    std::uint32_t anongame_string = 0;  // e.g. 'TEAM' / 'SOLO' / '2VS2'
    std::uint8_t  totalplayers    = 0;
    std::uint8_t  totalteams      = 0;
    std::uint16_t unknown2        = 0;
    std::uint8_t  visibility      = 0;  // 0x01 dark / 0x02 default
    std::uint8_t  unknown3        = 0;  // typically 0x02
    bool operator==(const SafPt2&) const = default;
};

struct AnonGameFound {
    std::uint32_t              count     = 0;
    std::uint32_t              unknown1  = 0;
    std::uint32_t              ip_be     = 0;  // wire BE — kept raw
    std::uint16_t              port_be   = 0;  // wire BE — kept raw
    std::uint8_t               unknown2  = 0;
    std::uint8_t               unknown3  = 0;
    std::uint16_t              unknown4  = 0;
    std::uint32_t              id        = 0;
    std::uint8_t               unknown5  = 0;  // typically 0x06
    std::uint8_t               type      = 0;
    std::uint8_t               gametype  = 0;
    std::string                mapname;
    SafPt2                     saf;            // 14-byte typed trailer
    std::vector<std::uint8_t>  extras;          // forward-compat: any bytes after saf
    bool operator==(const AnonGameFound&) const = default;
};

// --- 0x03 server: PG cancel ----------------------------------------------
struct AnonGameServerCancel {
    // legacy layout: cancel(u8) + count(u32) — but our envelope already
    // strips the leading sub_option byte, so what arrives here is just the
    // count.
    std::uint32_t count = 0;
    bool operator==(const AnonGameServerCancel&) const = default;
};

// --- 0x02 server: INFOREPLY ----------------------------------------------
// Legacy emits ONE item per packet (count=1, noitems=1, one tag+tag_unk
// header, variable payload, then a trailing 0x00 (last) or 0x01 (more))
// — see `handle_anongame.cpp::_client_findanongame_infos`. We model that
// shape directly: typed prefix + opaque tag-specific payload + trailing
// byte. Tag is the bytes the server actually emits (e.g.
// `kAnonGameInfoTagServerURL` = 'LRU\0' for a URL reply).
struct AnonGameInfoReply {
    std::uint32_t              count    = 0;
    std::uint8_t               noitems  = 0;
    std::uint32_t              tag      = 0;
    std::uint32_t              tag_unk  = 0;
    std::vector<std::uint8_t>  payload;          // tag-specific opaque blob
    std::uint8_t               trailing = 0;     // 0x00=last 0x01=more
    bool operator==(const AnonGameInfoReply&) const = default;
};

// --- 0x04 server: PROFILE2 ----------------------------------------------
struct AnonGameProfileReply {
    std::uint32_t              count    = 0;
    std::uint32_t              icon     = 0;
    std::uint8_t               rescount = 0;
    std::vector<std::uint8_t>  data;
    bool operator==(const AnonGameProfileReply&) const = default;
};

// --- 0x08 server: CLAN_PROFILE reply -------------------------------------
// Legacy `_client_anongame_profile_clan` always emits:
//   option(1)=0x08, count(u32 LE), rescount(u8)=0, trailer(1 byte 0x00).
// The post-rescount payload is kept as an opaque `trailer` blob so a
// future clan-stats implementation can slot real data in without
// reshaping the typed model.
struct AnonGameClanProfileReply {
    std::uint32_t              count    = 0;
    std::uint8_t               rescount = 0;
    std::vector<std::uint8_t>  trailer;
    bool operator==(const AnonGameClanProfileReply&) const = default;
};

// --- 0x09 server: ICONREPLY -----------------------------------------------
// Wire layout (after the 0x44/0x09 envelope strips the sub_option byte):
//   count            (u32 LE)   echo of request count
//   curricon         (4 raw)    e.g. "1H3W"
//   table_width      (u8)       5 (WAR3) or 6 (W3XP)
//   table_size       (u8)       table_width * table_height (4 or 5)
//   entries[N]:                 N == table_size
//     icon_code      (4 raw)    e.g. "2H3W"
//     portrait_code  (u32 LE)
//     race           (u8)
//     required_wins  (u16 BE)   *** big-endian on the wire (legacy bn_short)
//     client_enabled (u8)       0 or 1
struct AnonGameIconReplyEntry {
    std::array<char, 4> icon_code{};
    std::uint32_t       portrait_code  = 0;  // LE on wire
    std::uint8_t        race           = 0;
    std::uint16_t       required_wins  = 0;  // BE on wire
    std::uint8_t        client_enabled = 0;
    bool operator==(const AnonGameIconReplyEntry&) const = default;
};

struct AnonGameIconReply {
    std::uint32_t                          count = 0;
    std::array<char, 4>                    curricon{};
    std::uint8_t                           table_width  = 0;
    std::uint8_t                           table_size   = 0;  // == width * height
    std::vector<AnonGameIconReplyEntry>    entries;
    bool operator==(const AnonGameIconReply&) const = default;
};

// --- 0x07 server: tournament reply ---------------------------------------
struct AnonGameTournamentReply {
    std::uint32_t count    = 0;
    std::uint8_t  type     = 0;
    std::uint8_t  unknown1 = 0;
    std::uint16_t unknown4 = 0;
    std::uint32_t timestamp = 0;
    std::uint8_t  unknown5 = 0;
    std::uint16_t countdown = 0;
    std::uint16_t unknown2 = 0;
    std::uint8_t  wins     = 0;
    std::uint8_t  losses   = 0;
    std::uint8_t  ties     = 0;
    std::uint8_t  unknown3 = 0;
    std::uint8_t  selection = 0;
    std::uint8_t  descnum   = 0;
    std::uint8_t  nulltag   = 0;
    bool operator==(const AnonGameTournamentReply&) const = default;
};

// --- discriminated variants ----------------------------------------------
using AnonGameClient = std::variant<
    AnonGameSearch,
    AnonGameAtSearch,
    AnonGameAtInviterSearch,
    AnonGameInfoRequest,
    AnonGameClientCancel,
    AnonGameProfileRequest,
    AnonGameTournamentRequest,
    AnonGameClanProfileRequest,
    AnonGameGetIcon,
    AnonGameSetIcon>;

using AnonGameServer = std::variant<
    AnonGameSearchReply,
    AnonGameFound,
    AnonGameServerCancel,
    AnonGameInfoReply,
    AnonGameProfileReply,
    AnonGameTournamentReply,
    AnonGameIconReply,
    AnonGameClanProfileReply>;

// --- parse / serialize ---------------------------------------------------

/// Parse the typed sub-message from a received 0x44 client envelope.
core::Result<AnonGameClient> parse_findanongame_request(
    const WarcraftGeneralRequest& env);

/// Parse the typed sub-message from a received 0x44 server envelope.
core::Result<AnonGameServer> parse_findanongame_reply(
    const WarcraftGeneralReply& env);

/// Serialize a typed sub-message back into a 0x44 envelope.
WarcraftGeneralRequest serialize_findanongame_request(const AnonGameClient&);
WarcraftGeneralReply   serialize_findanongame_reply(const AnonGameServer&);

}  // namespace pvpgn::protocol::bnet
