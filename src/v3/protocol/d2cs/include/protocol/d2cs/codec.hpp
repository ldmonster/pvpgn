// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Minimal codec for the D2CS client protocol (Diablo II realm server).
///
/// Wire layout — 3-byte header, all LE:
///   u16 size   ─┐  total packet length, header included
///   u8  type   ─┘  e.g. 0x01 D2CS_LOGINREQ
///
/// This module only covers the login round-trip for now; create-char /
/// create-game / join-game land alongside Phase-5 realm wiring.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::d2cs {

inline constexpr std::uint8_t kClientLoginReq    = 0x01;
inline constexpr std::uint8_t kClientLoginReply  = 0x01;  // shared id

// 0x02 — create-char (both directions share the type code; v3 keeps
// them in separate Client/Server variants so the router stays unambiguous)
inline constexpr std::uint8_t kClientCreateCharReq   = 0x02;
inline constexpr std::uint8_t kClientCreateCharReply = 0x02;

// 0x03 — create-game
inline constexpr std::uint8_t kClientCreateGameReq   = 0x03;
inline constexpr std::uint8_t kClientCreateGameReply = 0x03;

// 0x04 — join-game
inline constexpr std::uint8_t kClientJoinGameReq     = 0x04;
inline constexpr std::uint8_t kClientJoinGameReply   = 0x04;

// 0x05 — game-list (public games)
inline constexpr std::uint8_t kClientGameListReq     = 0x05;
inline constexpr std::uint8_t kClientGameListReply   = 0x05;

// 0x06 — game-info (detail for one game)
inline constexpr std::uint8_t kClientGameInfoReq     = 0x06;
inline constexpr std::uint8_t kClientGameInfoReply   = 0x06;

// 0x17 — char-list
inline constexpr std::uint8_t kClientCharListReq     = 0x17;
inline constexpr std::uint8_t kClientCharListReply   = 0x17;
// 0x07 -- server -> client char-login result (no client request variant
// of this opcode exists; this is the bnetd-relayed reply to
// CLIENT_D2CS_CHARLOGINREQ)
inline constexpr std::uint8_t kClientCharLoginReply  = 0x07;
struct D2csHeader {
    std::uint16_t size = 0;
    std::uint8_t  type = 0;
    static constexpr std::size_t kSize = 3;
    bool operator==(const D2csHeader&) const = default;
};

struct LoginReq {
    std::uint32_t              seqno         = 0;
    std::uint32_t              u1            = 0;  ///< padding, legacy
    std::uint32_t              bncs_addr1    = 0;
    std::uint32_t              session_num   = 0;
    std::uint32_t              session_key   = 0;  ///< always 0 client-side
    std::uint32_t              cdkey_id      = 0;
    std::uint32_t              u5            = 0;  ///< padding, legacy
    std::uint32_t              client_tag    = 0;
    std::uint32_t              bn_version    = 0;
    std::uint32_t              bncs_addr2    = 0;
    std::uint32_t              u6            = 0;  ///< padding, legacy
    std::array<std::uint32_t,5> secret_hash{};
    std::string                account_name;
    bool operator==(const LoginReq&) const = default;
};

inline constexpr std::uint32_t kLoginReplyOk      = 0x00;
inline constexpr std::uint32_t kLoginReplyBadPass = 0x0C;

struct LoginReply {
    std::uint32_t reply = kLoginReplyOk;
    bool operator==(const LoginReply&) const = default;
};

// ---- 0x02 CREATECHAR ----------------------------------------------------

struct CreateCharReq {
    std::uint16_t chclass = 0;
    std::uint16_t u1      = 0;
    std::uint16_t status  = 0;
    std::string   name;
    bool operator==(const CreateCharReq&) const = default;
};

inline constexpr std::uint32_t kCreateCharReplyOk            = 0x00;
inline constexpr std::uint32_t kCreateCharReplyFailed        = 0x01;
inline constexpr std::uint32_t kCreateCharReplyAlreadyExists = 0x14;
inline constexpr std::uint32_t kCreateCharReplyNameRejected  = 0x15;

struct CreateCharReply {
    std::uint32_t reply = kCreateCharReplyOk;
    bool operator==(const CreateCharReply&) const = default;
};

// ---- 0x07 CHARLOGINREPLY (server -> client only) ------------------------

inline constexpr std::uint32_t kCharLoginReplySucceed  = 0x00;
inline constexpr std::uint32_t kCharLoginReplyFailed   = 0x01;
inline constexpr std::uint32_t kCharLoginReplyNotFound = 0x46;
inline constexpr std::uint32_t kCharLoginReplyExpired  = 0x7B;

struct CharLoginReply {
    std::uint32_t reply = kCharLoginReplySucceed;
    bool operator==(const CharLoginReply&) const = default;
};

// ---- 0x03 CREATEGAME ----------------------------------------------------

struct CreateGameReq {
    std::uint16_t seqno     = 0;
    std::uint32_t gameflag  = 0;
    std::uint8_t  u1        = 1;
    std::uint8_t  leveldiff = 0;
    std::uint8_t  maxchar   = 0;
    std::string   game_name;
    std::string   game_pass;
    std::string   game_desc;
    bool operator==(const CreateGameReq&) const = default;
};

inline constexpr std::uint32_t kCreateGameReplyOk           = 0x00;
inline constexpr std::uint32_t kCreateGameReplyFailed       = 0x01;
inline constexpr std::uint32_t kCreateGameReplyInvalidName  = 0x1E;
inline constexpr std::uint32_t kCreateGameReplyNameExists   = 0x1F;
inline constexpr std::uint32_t kCreateGameReplyServerDown   = 0x20;
inline constexpr std::uint32_t kCreateGameReplyUnavailable  = 0x32;

struct CreateGameReply {
    std::uint16_t seqno    = 0;
    std::uint16_t gameid   = 0;
    std::uint16_t u1       = 0;
    std::uint32_t reply    = kCreateGameReplyOk;
    bool operator==(const CreateGameReply&) const = default;
};

// ---- 0x04 JOINGAME ------------------------------------------------------

struct JoinGameReq {
    std::uint16_t seqno = 0;
    std::string   game_name;
    std::string   game_pass;
    bool operator==(const JoinGameReq&) const = default;
};

inline constexpr std::uint32_t kJoinGameReplyOk       = 0x00;
inline constexpr std::uint32_t kJoinGameReplyFailed   = 0x01;
inline constexpr std::uint32_t kJoinGameReplyBadPass  = 0x29;
inline constexpr std::uint32_t kJoinGameReplyNotFound = 0x2A;
inline constexpr std::uint32_t kJoinGameReplyFull     = 0x2B;
inline constexpr std::uint32_t kJoinGameReplyLevel    = 0x2C;

struct JoinGameReply {
    std::uint16_t seqno  = 0;
    std::uint16_t gameid = 0;
    std::uint16_t u1     = 0;
    std::uint32_t addr   = 0;
    std::uint32_t token  = 0;
    std::uint32_t reply  = kJoinGameReplyOk;
    bool operator==(const JoinGameReply&) const = default;
};

// ---- 0x05 GAMELIST ------------------------------------------------------

struct GameListReq {
    std::uint16_t seqno    = 0;
    std::uint32_t gameflag = 0;   ///< only hardcore bit set
    bool operator==(const GameListReq&) const = default;
};

/// One game per reply packet. The realm server emits one
/// ``GameListReply`` per public game; the codec round-trips a single
/// entry. An empty list terminates with ``token == 0`` and empty name.
struct GameListReply {
    std::uint16_t seqno    = 0;
    std::uint32_t token    = 0;
    std::uint8_t  currchar = 0;
    std::uint32_t gameflag = 0;
    std::string   game_name;
    std::string   game_desc;
    bool operator==(const GameListReply&) const = default;
};

// ---- 0x06 GAMEINFO ------------------------------------------------------

struct GameInfoReq {
    std::uint16_t seqno = 0;
    std::string   game_name;
    bool operator==(const GameInfoReq&) const = default;
};

struct GameInfoReply {
    std::uint16_t seqno     = 0;
    std::uint32_t gameflag  = 0;
    std::uint32_t etime     = 0;
    std::uint8_t  charlevel = 0;
    std::uint8_t  leveldiff = 0;
    std::uint8_t  maxchar   = 0;
    std::uint8_t  currchar  = 0;
    std::array<std::uint8_t, 16> chclass{};
    std::array<std::uint8_t, 16> charlevels{};
    std::string                  game_desc;
    /// One cstring per joined character (``currchar`` entries).
    std::vector<std::string>     char_names;
    bool operator==(const GameInfoReply&) const = default;
};

// ---- 0x17 CHARLIST ------------------------------------------------------

struct CharListReq {
    std::uint16_t maxchar = 0;
    std::uint16_t u1      = 0;
    bool operator==(const CharListReq&) const = default;
};

/// One character entry in a SERVER_CHARLISTREPLY. ``portrait`` is the
/// 34-byte opaque blob the legacy code described as "character portrait
/// block, 0x22 bytes static length" — kept opaque so the codec doesn't
/// presume an inventory layout.
struct CharListEntry {
    std::string                  name;
    std::array<std::uint8_t, 34> portrait{};
    bool operator==(const CharListEntry&) const = default;
};

struct CharListReply {
    std::uint16_t                 maxchar    = 0;
    std::uint16_t                 currchar   = 0;
    std::uint16_t                 u1         = 0;
    std::uint16_t                 currchar2  = 0;
    std::vector<CharListEntry>    chars;
    bool operator==(const CharListReply&) const = default;
};

using ClientMessage = std::variant<
    LoginReq, CreateCharReq, CreateGameReq, JoinGameReq,
    GameListReq, GameInfoReq, CharListReq>;
using ServerMessage = std::variant<
    LoginReply, CreateCharReply, CreateGameReply, JoinGameReply,
    GameListReply, GameInfoReply, CharListReply>;

core::Result<D2csHeader>    parse_header(core::ByteView buf);
core::Result<ClientMessage> decode_client(core::ByteView buf);
core::Result<ServerMessage> decode_server(core::ByteView buf);

core::Status<> encode(Writer& w, const LoginReq&        m);
core::Status<> encode(Writer& w, const LoginReply&      m);
core::Status<> encode(Writer& w, const CreateCharReq&   m);
core::Status<> encode(Writer& w, const CreateCharReply& m);
core::Status<> encode(Writer& w, const CharLoginReply&  m);
core::Status<> encode(Writer& w, const CreateGameReq&   m);
core::Status<> encode(Writer& w, const CreateGameReply& m);
core::Status<> encode(Writer& w, const JoinGameReq&     m);
core::Status<> encode(Writer& w, const JoinGameReply&   m);
core::Status<> encode(Writer& w, const GameListReq&     m);
core::Status<> encode(Writer& w, const GameListReply&   m);
core::Status<> encode(Writer& w, const GameInfoReq&     m);
core::Status<> encode(Writer& w, const GameInfoReply&   m);
core::Status<> encode(Writer& w, const CharListReq&     m);
core::Status<> encode(Writer& w, const CharListReply&   m);

}  // namespace pvpgn::protocol::d2cs
