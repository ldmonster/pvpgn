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

using ClientMessage = std::variant<
    LoginReq, CreateCharReq, CreateGameReq, JoinGameReq>;
using ServerMessage = std::variant<
    LoginReply, CreateCharReply, CreateGameReply, JoinGameReply>;

core::Result<D2csHeader>    parse_header(core::ByteView buf);
core::Result<ClientMessage> decode_client(core::ByteView buf);
core::Result<ServerMessage> decode_server(core::ByteView buf);

core::Status<> encode(Writer& w, const LoginReq&        m);
core::Status<> encode(Writer& w, const LoginReply&      m);
core::Status<> encode(Writer& w, const CreateCharReq&   m);
core::Status<> encode(Writer& w, const CreateCharReply& m);
core::Status<> encode(Writer& w, const CreateGameReq&   m);
core::Status<> encode(Writer& w, const CreateGameReply& m);
core::Status<> encode(Writer& w, const JoinGameReq&     m);
core::Status<> encode(Writer& w, const JoinGameReply&   m);

}  // namespace pvpgn::protocol::d2cs
