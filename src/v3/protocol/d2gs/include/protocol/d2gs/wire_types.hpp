// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// D2CS<->D2GS internal protocol, mirrored from
/// `src/common/d2cs_d2gs_protocol.h`.
///
/// All multi-byte fields are LE on the wire. The codec serializes
/// each field via Reader/Writer; v3 wire structs do NOT match the
/// PACKED legacy layout in `sizeof`. Use the `kWireBytes*`
/// constants for explicit byte counts.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::d2gs::wire {

/// Common 8-byte header for every d2cs<->d2gs message.
struct Header
{
    std::uint16_t size  = 0;
    std::uint16_t type  = 0;
    std::uint32_t seqno = 0;
    constexpr bool operator==(const Header&) const = default;
};
static_assert(sizeof(Header) == 8);
static_assert(std::is_trivially_copyable_v<Header>);

// ---- Message type codes -----------------------------------------------

inline constexpr std::uint16_t kD2csD2gsAuthReq         = 0x10;
inline constexpr std::uint16_t kD2gsD2csAuthReply       = 0x11;
inline constexpr std::uint16_t kD2csD2gsAuthReply       = 0x11;
inline constexpr std::uint16_t kD2gsD2csSetGsInfo       = 0x12;
inline constexpr std::uint16_t kD2csD2gsSetGsInfo       = 0x12;
inline constexpr std::uint16_t kD2csD2gsEchoReq         = 0x13;
inline constexpr std::uint16_t kD2gsD2csEchoReply       = 0x13;
inline constexpr std::uint16_t kD2csD2gsControl         = 0x14;
inline constexpr std::uint16_t kD2csD2gsSetInitInfo     = 0x15;
inline constexpr std::uint16_t kD2csD2gsSetConfFile     = 0x16;
inline constexpr std::uint16_t kD2csD2gsCreateGameReq   = 0x20;
inline constexpr std::uint16_t kD2gsD2csCreateGameReply = 0x20;
inline constexpr std::uint16_t kD2csD2gsJoinGameReq     = 0x21;
inline constexpr std::uint16_t kD2gsD2csJoinGameReply   = 0x21;
inline constexpr std::uint16_t kD2gsD2csUpdateGameInfo  = 0x22;
inline constexpr std::uint16_t kD2gsD2csCloseGame       = 0x23;

// ---- AuthReply reply codes --------------------------------------------

inline constexpr std::uint32_t kAuthReplySucceed     = 0x00;
inline constexpr std::uint32_t kAuthReplyBadVersion  = 0x01;
inline constexpr std::uint32_t kAuthReplyBadChecksum = 0x02;

// ---- Control commands -------------------------------------------------

inline constexpr std::uint32_t kControlCmdRestart  = 0x01;
inline constexpr std::uint32_t kControlCmdShutdown = 0x02;
inline constexpr std::uint32_t kControlValueDefault= 0x00;

// ---- Difficulty codes (also re-exported under `d2gs::wire` from
//      D2GAME_DIFFICULTY_*) --------------------------------------------

inline constexpr std::uint8_t kDifficultyNormal    = 0;
inline constexpr std::uint8_t kDifficultyNightmare = 1;
inline constexpr std::uint8_t kDifficultyHell      = 2;

// ---- CreateGame reply codes -------------------------------------------

inline constexpr std::uint32_t kCreateGameSucceed = 0;
inline constexpr std::uint32_t kCreateGameFailed  = 1;

// ---- JoinGame reply codes ---------------------------------------------

inline constexpr std::uint32_t kJoinGameSucceed  = 0;
inline constexpr std::uint32_t kJoinGameFailed   = 1;
inline constexpr std::uint32_t kJoinGameGameFull = 2;

// ---- UpdateGameInfo flags --------------------------------------------

inline constexpr std::uint32_t kUpdateGameInfoFlagUpdate = 0;
inline constexpr std::uint32_t kUpdateGameInfoFlagEnter  = 1;
inline constexpr std::uint32_t kUpdateGameInfoFlagLeave  = 2;

// ---- Message bodies (fixed prefixes) ----------------------------------

struct AuthReq
{
    Header        h{};
    std::uint32_t sessionnum = 0;
    std::uint32_t signlen    = 0;
    constexpr bool operator==(const AuthReq&) const = default;
};

struct AuthReplyFromD2gs
{
    Header                         h{};
    std::uint32_t                  version  = 0;
    std::uint32_t                  checksum = 0;
    std::uint32_t                  randnum  = 0;
    std::uint32_t                  signlen  = 0;
    std::array<std::uint8_t, 128>  sign{};
    constexpr bool operator==(const AuthReplyFromD2gs&) const = default;
};

struct AuthReplyFromD2cs
{
    Header        h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const AuthReplyFromD2cs&) const = default;
};

struct SetGsInfo
{
    Header        h{};
    std::uint32_t maxgame  = 0;
    std::uint32_t gameflag = 0;
    constexpr bool operator==(const SetGsInfo&) const = default;
};

struct EchoReq
{
    Header h{};
    constexpr bool operator==(const EchoReq&) const = default;
};

struct EchoReply
{
    Header h{};
    constexpr bool operator==(const EchoReply&) const = default;
};

struct Control
{
    Header        h{};
    std::uint32_t cmd   = 0;
    std::uint32_t value = 0;
    constexpr bool operator==(const Control&) const = default;
};

struct SetInitInfo
{
    Header        h{};
    std::uint32_t time       = 0;
    std::uint32_t gs_id      = 0;
    std::uint32_t ac_version = 0;
    constexpr bool operator==(const SetInitInfo&) const = default;
};

struct SetConfFile
{
    Header        h{};
    std::uint32_t size      = 0;
    std::uint32_t reserved1 = 0;
    constexpr bool operator==(const SetConfFile&) const = default;
};

struct CreateGameReq
{
    Header       h{};
    std::uint8_t ladder     = 0;
    std::uint8_t expansion  = 0;
    std::uint8_t difficulty = 0;
    std::uint8_t hardcore   = 0;
    constexpr bool operator==(const CreateGameReq&) const = default;
};

struct CreateGameReply
{
    Header        h{};
    std::uint32_t result = 0;
    std::uint32_t gameid = 0;
    constexpr bool operator==(const CreateGameReply&) const = default;
};

struct JoinGameReq
{
    Header        h{};
    std::uint32_t gameid = 0;
    std::uint32_t token  = 0;
    constexpr bool operator==(const JoinGameReq&) const = default;
};

struct JoinGameReply
{
    Header        h{};
    std::uint32_t result = 0;
    std::uint32_t gameid = 0;
    constexpr bool operator==(const JoinGameReply&) const = default;
};

struct UpdateGameInfo
{
    Header        h{};
    std::uint32_t flag      = 0;
    std::uint32_t gameid    = 0;
    std::uint32_t charlevel = 0;
    std::uint32_t charclass = 0;
    constexpr bool operator==(const UpdateGameInfo&) const = default;
};

struct CloseGame
{
    Header        h{};
    std::uint32_t gameid = 0;
    constexpr bool operator==(const CloseGame&) const = default;
};

static_assert(std::is_trivially_copyable_v<AuthReplyFromD2gs>);
static_assert(std::is_trivially_copyable_v<UpdateGameInfo>);

}  // namespace pvpgn::protocol::d2gs::wire
