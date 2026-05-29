// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// Client <-> D2CS wire types, mirrored from
/// `src/common/d2cs_protocol.h`.
///
/// All multi-byte fields use native fixed-width integers; the codec
/// layer in `protocol/common/` is responsible for the LE byte-order
/// conversion. Variable-length trailing data (account names, game
/// names, character names, descriptions, blobs) is NOT represented in
/// these structs and must be handled by the codec.

#include <array>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::d2cs::wire {

// ---- Message type codes ------------------------------------------------

inline constexpr std::uint8_t kClientLoginReq          = 0x01;
inline constexpr std::uint8_t kLoginReply              = 0x01;
inline constexpr std::uint8_t kClientCreateCharReq     = 0x02;
inline constexpr std::uint8_t kCreateCharReply         = 0x02;
inline constexpr std::uint8_t kClientCreateGameReq     = 0x03;
inline constexpr std::uint8_t kCreateGameReply         = 0x03;
inline constexpr std::uint8_t kClientJoinGameReq       = 0x04;
inline constexpr std::uint8_t kJoinGameReply           = 0x04;
inline constexpr std::uint8_t kClientGameListReq       = 0x05;
inline constexpr std::uint8_t kGameListReply           = 0x05;
inline constexpr std::uint8_t kClientGameInfoReq       = 0x06;
inline constexpr std::uint8_t kGameInfoReply           = 0x06;
inline constexpr std::uint8_t kClientCharLoginReq      = 0x07;
inline constexpr std::uint8_t kCharLoginReply          = 0x07;
inline constexpr std::uint8_t kClientDeleteCharReq     = 0x0a;
inline constexpr std::uint8_t kDeleteCharReply         = 0x0a;
inline constexpr std::uint8_t kClientLadderReq         = 0x11;
inline constexpr std::uint8_t kLadderReply             = 0x11;
inline constexpr std::uint8_t kClientMotdReq           = 0x12;
inline constexpr std::uint8_t kMotdReply               = 0x12;
inline constexpr std::uint8_t kClientCancelCreateGame  = 0x13;
inline constexpr std::uint8_t kCreateGameWait          = 0x14;
inline constexpr std::uint8_t kCharLadderReq           = 0x16;
inline constexpr std::uint8_t kClientCharListReq       = 0x17;
inline constexpr std::uint8_t kCharListReply           = 0x17;
inline constexpr std::uint8_t kClientConvertCharReq    = 0x18;
inline constexpr std::uint8_t kConvertCharReply        = 0x18;
inline constexpr std::uint8_t kClientCharListReq110    = 0x19;
inline constexpr std::uint8_t kCharListReply110        = 0x19;

// ---- Reply codes -------------------------------------------------------

inline constexpr std::uint32_t kLoginReplySucceed         = 0x00;
inline constexpr std::uint32_t kLoginReplyBadPass         = 0x0c;

inline constexpr std::uint32_t kCreateCharReplySucceed       = 0x00;
inline constexpr std::uint32_t kCreateCharReplyFailed        = 0x01;
inline constexpr std::uint32_t kCreateCharReplyAlreadyExist  = 0x14;
inline constexpr std::uint32_t kCreateCharReplyNameReject    = 0x15;

inline constexpr std::uint32_t kCreateGameReplySucceed       = 0x00;
inline constexpr std::uint32_t kCreateGameReplyFailed        = 0x01;
inline constexpr std::uint32_t kCreateGameReplyInvalidName   = 0x1e;
inline constexpr std::uint32_t kCreateGameReplyNameExist     = 0x1f;
inline constexpr std::uint32_t kCreateGameReplyServerDown    = 0x20;
inline constexpr std::uint32_t kCreateGameReplyNotAvailable  = 0x32;
inline constexpr std::uint32_t kCreateGameReplyU1            = 0x33;

inline constexpr std::uint32_t kJoinGameReplySucceed             = 0x00;
inline constexpr std::uint32_t kJoinGameReplyBadPass             = 0x29;
inline constexpr std::uint32_t kJoinGameReplyNotExist            = 0x2a;
inline constexpr std::uint32_t kJoinGameReplyGameFull            = 0x2b;
inline constexpr std::uint32_t kJoinGameReplyLevelLimit          = 0x2c;
inline constexpr std::uint32_t kJoinGameReplyHardcoreSoftcore    = 0x71;
inline constexpr std::uint32_t kJoinGameReplyNormalNightmare     = 0x73;
inline constexpr std::uint32_t kJoinGameReplyNightmareHell       = 0x74;
inline constexpr std::uint32_t kJoinGameReplyClassicExpansion    = 0x78;
inline constexpr std::uint32_t kJoinGameReplyExpansionClassic    = 0x79;
inline constexpr std::uint32_t kJoinGameReplyNormalLadder        = 0x7D;
inline constexpr std::uint32_t kJoinGameReplyFailed              = 0x01;

inline constexpr std::uint32_t kCharLoginReplySucceed   = 0x00;
inline constexpr std::uint32_t kCharLoginReplyFailed    = 0x01;
inline constexpr std::uint32_t kCharLoginReplyNotFound  = 0x46;
inline constexpr std::uint32_t kCharLoginReplyExpired   = 0x7b;

inline constexpr std::uint32_t kDeleteCharReplySucceed  = 0x00;
inline constexpr std::uint32_t kDeleteCharReplyFailed   = 0x01;

inline constexpr std::uint32_t kConvertCharReplySucceed = 0x00;
inline constexpr std::uint32_t kConvertCharReplyFailed  = 0x01;

// ---- Ladder status flags ----------------------------------------------

inline constexpr std::uint16_t kLadderStatusDead       = 0x10;
inline constexpr std::uint16_t kLadderStatusHardcore   = 0x20;
inline constexpr std::uint16_t kLadderStatusExpansion  = 0x40;
inline constexpr std::uint16_t kLadderStatusDifficulty = 0x0f00;

// ---- Header -----------------------------------------------------------

struct ClientHeader
{
    std::uint16_t size = 0;
    std::uint8_t  type = 0;
    constexpr bool operator==(const ClientHeader&) const = default;
};
inline constexpr std::size_t kWireBytesClientHeader = 3;
static_assert(std::is_trivially_copyable_v<ClientHeader>);

// ---- Message structs (header + fixed part; variable tail omitted) -----

struct ClientLoginReq
{
    ClientHeader  h{};
    std::uint32_t seqno       = 0;
    std::uint32_t u1          = 0;
    std::uint32_t bncs_addr1  = 0;
    std::uint32_t sessionnum  = 0;
    std::uint32_t sessionkey  = 0;
    std::uint32_t cdkey_id    = 0;
    std::uint32_t u5          = 0;
    std::uint32_t clienttag   = 0;
    std::uint32_t bnversion   = 0;
    std::uint32_t bncs_addr2  = 0;
    std::uint32_t u6          = 0;
    std::array<std::uint32_t, 5> secret_hash{};
    constexpr bool operator==(const ClientLoginReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientLoginReq = 3 + 4 * 11 + 4 * 5;  // 67
static_assert(std::is_trivially_copyable_v<ClientLoginReq>);

struct LoginReply
{
    ClientHeader  h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const LoginReply&) const = default;
};
inline constexpr std::size_t kWireBytesLoginReply = 3 + 4;
static_assert(std::is_trivially_copyable_v<LoginReply>);

struct ClientCreateCharReq
{
    ClientHeader  h{};
    std::uint16_t chclass = 0;
    std::uint16_t u1      = 0;
    std::uint16_t status  = 0;
    constexpr bool operator==(const ClientCreateCharReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientCreateCharReq = 3 + 2 * 3;
static_assert(std::is_trivially_copyable_v<ClientCreateCharReq>);

struct CreateCharReply
{
    ClientHeader  h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const CreateCharReply&) const = default;
};
inline constexpr std::size_t kWireBytesCreateCharReply = 3 + 4;
static_assert(std::is_trivially_copyable_v<CreateCharReply>);

struct ClientCreateGameReq
{
    ClientHeader  h{};
    std::uint16_t seqno     = 0;
    std::uint32_t gameflag  = 0;
    std::uint8_t  u1        = 0;
    std::uint8_t  leveldiff = 0;
    std::uint8_t  maxchar   = 0;
    constexpr bool operator==(const ClientCreateGameReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientCreateGameReq = 3 + 2 + 4 + 3;
static_assert(std::is_trivially_copyable_v<ClientCreateGameReq>);

struct CreateGameReply
{
    ClientHeader  h{};
    std::uint16_t seqno  = 0;
    std::uint16_t gameid = 0;
    std::uint16_t u1     = 0;
    std::uint32_t reply  = 0;
    constexpr bool operator==(const CreateGameReply&) const = default;
};
inline constexpr std::size_t kWireBytesCreateGameReply = 3 + 2 * 3 + 4;
static_assert(std::is_trivially_copyable_v<CreateGameReply>);

struct ClientJoinGameReq
{
    ClientHeader  h{};
    std::uint16_t seqno = 0;
    constexpr bool operator==(const ClientJoinGameReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientJoinGameReq = 3 + 2;
static_assert(std::is_trivially_copyable_v<ClientJoinGameReq>);

struct JoinGameReply
{
    ClientHeader  h{};
    std::uint16_t seqno   = 0;
    std::uint16_t gameid  = 0;
    std::uint16_t u1      = 0;
    std::uint32_t addr    = 0;
    std::uint32_t token   = 0;
    std::uint32_t reply   = 0;
    constexpr bool operator==(const JoinGameReply&) const = default;
};
inline constexpr std::size_t kWireBytesJoinGameReply = 3 + 2 * 3 + 4 * 3;
static_assert(std::is_trivially_copyable_v<JoinGameReply>);

struct ClientGameListReq
{
    ClientHeader  h{};
    std::uint16_t seqno    = 0;
    std::uint32_t gameflag = 0;
    constexpr bool operator==(const ClientGameListReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientGameListReq = 3 + 2 + 4;
static_assert(std::is_trivially_copyable_v<ClientGameListReq>);

struct GameListReply
{
    ClientHeader  h{};
    std::uint16_t seqno    = 0;
    std::uint32_t token    = 0;
    std::uint8_t  currchar = 0;
    std::uint32_t gameflag = 0;
    constexpr bool operator==(const GameListReply&) const = default;
};
inline constexpr std::size_t kWireBytesGameListReply = 3 + 2 + 4 + 1 + 4;
static_assert(std::is_trivially_copyable_v<GameListReply>);

struct ClientGameInfoReq
{
    ClientHeader  h{};
    std::uint16_t seqno = 0;
    constexpr bool operator==(const ClientGameInfoReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientGameInfoReq = 3 + 2;
static_assert(std::is_trivially_copyable_v<ClientGameInfoReq>);

struct GameInfoReply
{
    ClientHeader  h{};
    std::uint16_t seqno     = 0;
    std::uint32_t gameflag  = 0;
    std::uint32_t etime     = 0;
    std::uint8_t  charlevel = 0;
    std::uint8_t  leveldiff = 0;
    std::uint8_t  maxchar   = 0;
    std::uint8_t  currchar  = 0;
    std::array<std::uint8_t, 16> chclass{};
    std::array<std::uint8_t, 16> level{};
    constexpr bool operator==(const GameInfoReply&) const = default;
};
inline constexpr std::size_t kWireBytesGameInfoReply = 3 + 2 + 4 * 2 + 4 + 16 * 2;
static_assert(std::is_trivially_copyable_v<GameInfoReply>);

struct ClientCharLoginReq
{
    ClientHeader h{};
    constexpr bool operator==(const ClientCharLoginReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientCharLoginReq = 3;
static_assert(std::is_trivially_copyable_v<ClientCharLoginReq>);

struct CharLoginReply
{
    ClientHeader  h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const CharLoginReply&) const = default;
};
inline constexpr std::size_t kWireBytesCharLoginReply = 3 + 4;
static_assert(std::is_trivially_copyable_v<CharLoginReply>);

struct ClientDeleteCharReq
{
    ClientHeader  h{};
    std::uint16_t u1 = 0;
    constexpr bool operator==(const ClientDeleteCharReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientDeleteCharReq = 3 + 2;
static_assert(std::is_trivially_copyable_v<ClientDeleteCharReq>);

struct DeleteCharReply
{
    ClientHeader  h{};
    std::uint16_t u1    = 0;
    std::uint32_t reply = 0;
    constexpr bool operator==(const DeleteCharReply&) const = default;
};
inline constexpr std::size_t kWireBytesDeleteCharReply = 3 + 2 + 4;
static_assert(std::is_trivially_copyable_v<DeleteCharReply>);

struct ClientLadderReq
{
    ClientHeader  h{};
    std::uint8_t  type      = 0;
    std::uint16_t start_pos = 0;
    constexpr bool operator==(const ClientLadderReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientLadderReq = 3 + 1 + 2;
static_assert(std::is_trivially_copyable_v<ClientLadderReq>);

struct LadderReply
{
    ClientHeader  h{};
    std::uint8_t  type      = 0;
    std::uint16_t total_len = 0;
    std::uint16_t curr_len  = 0;
    std::uint16_t cont_len  = 0;
    constexpr bool operator==(const LadderReply&) const = default;
};
inline constexpr std::size_t kWireBytesLadderReply = 3 + 1 + 2 * 3;
static_assert(std::is_trivially_copyable_v<LadderReply>);

struct LadderHeader
{
    std::uint16_t start_pos = 0;
    std::uint16_t u1        = 0;
    std::uint32_t count1    = 0;
    constexpr bool operator==(const LadderHeader&) const = default;
};
static_assert(sizeof(LadderHeader) == 8);
static_assert(std::is_trivially_copyable_v<LadderHeader>);

struct LadderInfoHeader
{
    std::uint32_t count2 = 0;
    constexpr bool operator==(const LadderInfoHeader&) const = default;
};
static_assert(sizeof(LadderInfoHeader) == 4);
static_assert(std::is_trivially_copyable_v<LadderInfoHeader>);

struct LadderInfo
{
    std::uint32_t explow   = 0;
    std::uint32_t exphigh  = 0;
    std::uint16_t status   = 0;
    std::uint8_t  level    = 0;
    std::uint8_t  u1       = 0;
    std::array<char, 16> charname{};
    constexpr bool operator==(const LadderInfo&) const = default;
};
inline constexpr std::size_t kWireBytesLadderInfo = 4 * 2 + 2 + 1 + 1 + 16;  // 28
static_assert(sizeof(LadderInfo) == 28);
static_assert(std::is_trivially_copyable_v<LadderInfo>);

struct ClientMotdReq
{
    ClientHeader h{};
    constexpr bool operator==(const ClientMotdReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientMotdReq = 3;
static_assert(std::is_trivially_copyable_v<ClientMotdReq>);

struct MotdReply
{
    ClientHeader h{};
    std::uint8_t u1 = 0;
    constexpr bool operator==(const MotdReply&) const = default;
};
inline constexpr std::size_t kWireBytesMotdReply = 3 + 1;
static_assert(std::is_trivially_copyable_v<MotdReply>);

struct ClientCancelCreateGame
{
    ClientHeader h{};
    constexpr bool operator==(const ClientCancelCreateGame&) const = default;
};
inline constexpr std::size_t kWireBytesClientCancelCreateGame = 3;
static_assert(std::is_trivially_copyable_v<ClientCancelCreateGame>);

struct CreateGameWait
{
    ClientHeader  h{};
    std::uint32_t position = 0;
    constexpr bool operator==(const CreateGameWait&) const = default;
};
inline constexpr std::size_t kWireBytesCreateGameWait = 3 + 4;
static_assert(std::is_trivially_copyable_v<CreateGameWait>);

struct CharLadderReq
{
    ClientHeader  h{};
    std::uint32_t hardcore  = 0;
    std::uint32_t expansion = 0;
    constexpr bool operator==(const CharLadderReq&) const = default;
};
inline constexpr std::size_t kWireBytesCharLadderReq = 3 + 4 * 2;
static_assert(std::is_trivially_copyable_v<CharLadderReq>);

struct ClientCharListReq
{
    ClientHeader  h{};
    std::uint16_t maxchar = 0;
    std::uint16_t u1      = 0;
    constexpr bool operator==(const ClientCharListReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientCharListReq = 3 + 2 * 2;
static_assert(std::is_trivially_copyable_v<ClientCharListReq>);

struct CharListReply
{
    ClientHeader  h{};
    std::uint16_t maxchar    = 0;
    std::uint16_t currchar   = 0;
    std::uint16_t u1         = 0;
    std::uint16_t currchar2  = 0;
    constexpr bool operator==(const CharListReply&) const = default;
};
inline constexpr std::size_t kWireBytesCharListReply = 3 + 2 * 4;
static_assert(std::is_trivially_copyable_v<CharListReply>);

struct ClientConvertCharReq
{
    ClientHeader h{};
    constexpr bool operator==(const ClientConvertCharReq&) const = default;
};
inline constexpr std::size_t kWireBytesClientConvertCharReq = 3;
static_assert(std::is_trivially_copyable_v<ClientConvertCharReq>);

struct ConvertCharReply
{
    ClientHeader  h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const ConvertCharReply&) const = default;
};
inline constexpr std::size_t kWireBytesConvertCharReply = 3 + 4;
static_assert(std::is_trivially_copyable_v<ConvertCharReply>);

struct ClientCharListReq110
{
    ClientHeader  h{};
    std::uint16_t maxchar = 0;
    std::uint16_t u1      = 0;
    constexpr bool operator==(const ClientCharListReq110&) const = default;
};
inline constexpr std::size_t kWireBytesClientCharListReq110 = 3 + 2 * 2;
static_assert(std::is_trivially_copyable_v<ClientCharListReq110>);

struct CharListReply110
{
    ClientHeader  h{};
    std::uint16_t maxchar   = 0;
    std::uint16_t currchar  = 0;
    std::uint16_t u1        = 0;
    std::uint16_t currchar2 = 0;
    constexpr bool operator==(const CharListReply110&) const = default;
};
inline constexpr std::size_t kWireBytesCharListReply110 = 3 + 2 * 4;
static_assert(std::is_trivially_copyable_v<CharListReply110>);

struct CharData
{
    std::uint32_t expire_time = 0;
    constexpr bool operator==(const CharData&) const = default;
};
static_assert(sizeof(CharData) == 4);
static_assert(std::is_trivially_copyable_v<CharData>);

}  // namespace pvpgn::protocol::d2cs::wire
