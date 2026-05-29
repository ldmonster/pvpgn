// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_wire_types.hpp
/// Diablo II game-server <-> client wire types, mirrored from
/// `src/common/d2game_protocol.h`.
///
/// Lives next to `d2gs::wire` (server <-> d2cs/d2gs control protocol)
/// in a separate sub-namespace to keep type names short and to make
/// the protocol direction obvious at call sites.
///
/// In production builds the header is a single message-type byte
/// (the `NOTONLYER` "beta" variant with magic + size + type is not
/// used). Variable-length trailing data (character names, save
/// payloads, chat messages) is NOT modelled here; codec layer
/// handles it.

#include <array>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::d2gs::game {

// ---- Message type codes -----------------------------------------------

inline constexpr std::uint8_t kServer00            = 0x00;  // beta-only
inline constexpr std::uint8_t kClient01            = 0x01;  // beta-only
inline constexpr std::uint8_t kServerJoinOk        = 0x01;
inline constexpr std::uint8_t kClientChatMessage   = 0x15;
inline constexpr std::uint8_t kServerNoop          = 0x20;
inline constexpr std::uint8_t kServerChatMessage   = 0x26;
inline constexpr std::uint8_t kClientDie           = 0x41;
inline constexpr std::uint8_t kServerUnknown59     = 0x59;
inline constexpr std::uint8_t kServerJoinGameMsg   = 0x5a;
inline constexpr std::uint8_t kClientCreateGameReq = 0x60;
inline constexpr std::uint8_t kClientJoinGameReq   = 0x61;
inline constexpr std::uint8_t kClientQuitGame      = 0x62;
inline constexpr std::uint8_t kClientJoinActReq    = 0x64;
inline constexpr std::uint8_t kClientPlayerSave    = 0x65;
inline constexpr std::uint8_t kClientUnknown66     = 0x66;
inline constexpr std::uint8_t kServerUnknown8F     = 0x8f;
inline constexpr std::uint8_t kServerUnknown96     = 0x96;
inline constexpr std::uint8_t kServerWelcome       = 0x97;
inline constexpr std::uint8_t kServerCloseGame     = 0x98;
inline constexpr std::uint8_t kServerPlayerSave    = 0x9b;
inline constexpr std::uint8_t kServerError         = 0x9c;

// ---- Server error codes ----------------------------------------------

inline constexpr std::uint32_t kErrorUnknownFailure  = 0;
inline constexpr std::uint32_t kErrorCharVer         = 1;
inline constexpr std::uint32_t kErrorQuestData       = 2;
inline constexpr std::uint32_t kErrorWpData          = 3;
inline constexpr std::uint32_t kErrorStatData        = 4;
inline constexpr std::uint32_t kErrorSkillData       = 5;
inline constexpr std::uint32_t kErrorUnableEnter     = 6;
inline constexpr std::uint32_t kErrorInventoryData   = 7;
inline constexpr std::uint32_t kErrorDeadBody        = 8;
inline constexpr std::uint32_t kErrorHeader          = 9;
inline constexpr std::uint32_t kErrorHireables       = 10;
inline constexpr std::uint32_t kErrorIntroData       = 11;
inline constexpr std::uint32_t kErrorItem            = 12;
inline constexpr std::uint32_t kErrorDeadBodyItem    = 13;
inline constexpr std::uint32_t kErrorGenericBadFile  = 14;
inline constexpr std::uint32_t kErrorGameFull        = 15;
inline constexpr std::uint32_t kErrorGameVer         = 16;
inline constexpr std::uint32_t kErrorNightmare       = 17;
inline constexpr std::uint32_t kErrorHell            = 18;
inline constexpr std::uint32_t kErrorNormalHardcore  = 19;
inline constexpr std::uint32_t kErrorHardcoreNormal  = 20;
inline constexpr std::uint32_t kErrorDeadHardcore    = 21;

// ---- Chat message magic constants ------------------------------------

inline constexpr std::uint16_t kServerChatMessageUnknown1 = 0x0001;
inline constexpr std::uint32_t kServerChatMessageUnknown2 = 0x00000002;
inline constexpr std::uint16_t kServerChatMessageUnknown3 = 0x0000;
inline constexpr std::uint8_t  kServerChatMessageUnknown4 = 0x01;

// ---- Header ----------------------------------------------------------

/// Production-mode header: just the message-type byte. Beta NOTONLYER
/// variant (magic + size + type) is not represented; the codec
/// decides which variant to emit based on protocol version.
struct Header
{
    std::uint8_t type = 0;
    constexpr bool operator==(const Header&) const = default;
};
static_assert(sizeof(Header) == 1);
static_assert(std::is_trivially_copyable_v<Header>);

// 16-byte character name (NUL-terminated ASCII).
using CharName = std::array<char, 16>;

// ---- Server-direction messages ---------------------------------------

struct ServerWelcome
{
    Header h{};
    constexpr bool operator==(const ServerWelcome&) const = default;
};

struct Server00Req
{
    Header       h{};
    std::uint8_t unknown1 = 0;
    constexpr bool operator==(const Server00Req&) const = default;
};

struct ServerNoop
{
    Header h{};
    constexpr bool operator==(const ServerNoop&) const = default;
};

struct ServerJoinOk
{
    Header        h{};
    std::uint8_t  difficulty = 0;
    std::uint16_t gameflag   = 0;
    std::uint8_t  chtemplate = 0;
    std::uint16_t unknown1   = 0;
    std::uint16_t unknown2   = 0;
    constexpr bool operator==(const ServerJoinOk&) const = default;
};
inline constexpr std::size_t kWireBytesServerJoinOk = 1 + 1 + 2 + 1 + 2 + 2;

struct ServerUnknown59
{
    Header h{};
    constexpr bool operator==(const ServerUnknown59&) const = default;
};

struct ServerJoinGameMessage
{
    Header        h{};
    std::uint8_t  unknown1 = 0;
    std::uint8_t  unknown2 = 0;
    std::uint32_t unknown3 = 0;
    std::uint8_t  unknown4 = 0;
    constexpr bool operator==(const ServerJoinGameMessage&) const = default;
};
inline constexpr std::size_t kWireBytesServerJoinGameMessage = 1 + 1 + 1 + 4 + 1;

struct ServerChatMessage
{
    Header        h{};
    std::uint16_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
    std::uint16_t unknown3 = 0;
    std::uint8_t  unknown4 = 0;
    constexpr bool operator==(const ServerChatMessage&) const = default;
};
inline constexpr std::size_t kWireBytesServerChatMessage = 1 + 2 + 4 + 2 + 1;

struct ServerPlayerSave
{
    Header        h{};
    std::uint8_t  size       = 0;
    std::uint8_t  start      = 0;
    std::uint32_t total_size = 0;
    constexpr bool operator==(const ServerPlayerSave&) const = default;
};
inline constexpr std::size_t kWireBytesServerPlayerSave = 1 + 1 + 1 + 4;

struct ServerCloseGame
{
    Header        h{};
    std::uint16_t unknown1 = 0;
    constexpr bool operator==(const ServerCloseGame&) const = default;
};
inline constexpr std::size_t kWireBytesServerCloseGame = 1 + 2;

struct ServerError
{
    Header        h{};
    std::uint32_t errorno = 0;
    constexpr bool operator==(const ServerError&) const = default;
};
inline constexpr std::size_t kWireBytesServerError = 1 + 4;

struct ServerUnknown8F
{
    Header        h{};
    std::uint32_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
    std::uint32_t unknown3 = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::uint32_t unknown6 = 0;
    std::uint32_t unknown7 = 0;
    constexpr bool operator==(const ServerUnknown8F&) const = default;
};
inline constexpr std::size_t kWireBytesServerUnknown8F = 1 + 4 * 7;

// ---- Client-direction messages ---------------------------------------

struct Client01
{
    Header        h{};
    std::uint8_t  unknown1 = 0;
    std::uint32_t gameid1  = 0;
    std::uint16_t gameid2  = 0;
    std::array<std::uint8_t, 5> unknown2{};
    constexpr bool operator==(const Client01&) const = default;
};
inline constexpr std::size_t kWireBytesClient01 = 1 + 1 + 4 + 2 + 5;

struct ClientCreateGameReq
{
    Header        h{};
    CharName      gamename{};
    std::uint8_t  servertype = 0;
    std::uint8_t  chclass    = 0;
    std::uint8_t  chtemplate = 0;
    std::uint8_t  difficulty = 0;
    CharName      charname{};
    std::uint16_t arena      = 0;
    std::uint32_t gameflag   = 0;
    std::uint8_t  unknownb2  = 0;
    std::uint8_t  unknownb3  = 0;
    constexpr bool operator==(const ClientCreateGameReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientCreateGameReq =
    1 + 16 + 1 + 1 + 1 + 1 + 16 + 2 + 4 + 1 + 1;  // 45

struct GameFlag
{
    std::uint8_t flag1 = 0;
    std::uint8_t flag2 = 0;
    std::uint8_t flag3 = 0;
    std::uint8_t flag4 = 0;
    constexpr bool operator==(const GameFlag&) const = default;
};
static_assert(sizeof(GameFlag) == 4);

struct ClientJoinGameReq
{
    Header        h{};
    std::uint32_t token     = 0;
    std::uint16_t gameid    = 0;
    std::uint8_t  charclass = 0;  // 00=Amazon 01=Sor 02=Nec 03=Pal 04=Bar
    std::uint32_t version   = 0;
    CharName      charname{};
    constexpr bool operator==(const ClientJoinGameReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientJoinGameReq = 1 + 4 + 2 + 1 + 4 + 16;

struct ClientUnknown66
{
    Header        h{};
    std::uint32_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
    constexpr bool operator==(const ClientUnknown66&) const = default;
};
inline constexpr std::size_t kWireBytesClientUnknown66 = 1 + 4 + 4;

struct ClientJoinActReq
{
    Header h{};
    constexpr bool operator==(const ClientJoinActReq&) const = default;
};

struct ClientPlayerSave
{
    Header        h{};
    std::uint8_t  size       = 0;
    std::uint32_t total_size = 0;
    constexpr bool operator==(const ClientPlayerSave&) const = default;
};
inline constexpr std::size_t kWireBytesClientPlayerSave = 1 + 1 + 4;

struct ClientChatMessage
{
    Header        h{};
    std::uint16_t unknown1 = 0;
    constexpr bool operator==(const ClientChatMessage&) const = default;
};
inline constexpr std::size_t kWireBytesClientChatMessage = 1 + 2;

struct ClientQuitGame
{
    Header h{};
    constexpr bool operator==(const ClientQuitGame&) const = default;
};

struct ClientDie
{
    Header h{};
    constexpr bool operator==(const ClientDie&) const = default;
};

}  // namespace pvpgn::protocol::d2gs::game
