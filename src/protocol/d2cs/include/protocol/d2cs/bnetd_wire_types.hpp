// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnetd_wire_types.hpp
/// Bnetd<->D2CS internal protocol, mirrored from
/// `src/common/d2cs_bnetd_protocol.h`.
///
/// All multi-byte fields are LE on the wire. Wire structs are
/// trivially copyable and have explicit size assertions; the codec
/// layer reads/writes them field by field via Reader/Writer.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::d2cs::bnetd {

/// Common 8-byte header for every bnetd<->d2cs message.
struct Header
{
    std::uint16_t size  = 0;
    std::uint16_t type  = 0;
    std::uint32_t seqno = 0;   ///< set by sender
    constexpr bool operator==(const Header&) const = default;
};
static_assert(sizeof(Header) == 8);
static_assert(std::is_trivially_copyable_v<Header>);

// ---- Message type codes -----------------------------------------------

inline constexpr std::uint16_t kBnetdToD2csAuthReq          = 0x01;
inline constexpr std::uint16_t kD2csToBnetdAuthReply        = 0x02;
inline constexpr std::uint16_t kBnetdToD2csAuthReply        = 0x02;
inline constexpr std::uint16_t kD2csToBnetdAccountLoginReq  = 0x10;
inline constexpr std::uint16_t kBnetdToD2csAccountLoginReply= 0x10;
inline constexpr std::uint16_t kD2csToBnetdCharLoginReq     = 0x11;
inline constexpr std::uint16_t kBnetdToD2csCharLoginReply   = 0x11;
inline constexpr std::uint16_t kBnetdToD2csGameInfoReq      = 0x12;
inline constexpr std::uint16_t kD2csToBnetdGameInfoReply    = 0x12;

// ---- AuthReply reply codes --------------------------------------------

inline constexpr std::uint32_t kAuthReplySucceed     = 0x00;
inline constexpr std::uint32_t kAuthReplyBadVersion  = 0x01;

// ---- AccountLoginReply reply codes ------------------------------------

inline constexpr std::uint32_t kAccountLoginSucceed = 0x00;
inline constexpr std::uint32_t kAccountLoginFailed  = 0x01;

// ---- CharLoginReply reply codes ---------------------------------------

inline constexpr std::uint32_t kCharLoginSucceed = 0x00;
inline constexpr std::uint32_t kCharLoginFailed  = 0x01;

// ---- Fixed-prefix message bodies --------------------------------------
//
// Each struct represents the FIXED prefix of the on-wire message; any
// trailing variable-length payload (NUL-terminated names, etc.) is
// commented and handled by the codec.

/// `BNETD_D2CS_AUTHREQ` (0x01).
struct AuthReq
{
    Header        h{};
    std::uint32_t sessionnum = 0;
    constexpr bool operator==(const AuthReq&) const = default;
};
static_assert(sizeof(AuthReq) == 12);

/// `D2CS_BNETD_AUTHREPLY` (0x02) -- followed by NUL-terminated realm
/// name.
struct AuthReplyFromD2cs
{
    Header        h{};
    std::uint32_t version = 0;
    constexpr bool operator==(const AuthReplyFromD2cs&) const = default;
};
static_assert(sizeof(AuthReplyFromD2cs) == 12);

/// `BNETD_D2CS_AUTHREPLY` (0x02).
struct AuthReplyFromBnetd
{
    Header        h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const AuthReplyFromBnetd&) const = default;
};
static_assert(sizeof(AuthReplyFromBnetd) == 12);

/// `D2CS_BNETD_ACCOUNTLOGINREQ` (0x10) -- followed by NUL-terminated
/// account name.
struct AccountLoginReq
{
    Header                       h{};
    std::uint32_t                seqno_inner  = 0;
    std::uint32_t                sessionnum   = 0;
    std::uint32_t                sessionkey   = 0;
    std::array<std::uint32_t, 5> secret_hash{};
    constexpr bool operator==(const AccountLoginReq&) const = default;
};
static_assert(sizeof(AccountLoginReq) == 8 + 4 + 4 + 4 + 20);

/// `BNETD_D2CS_ACCOUNTLOGINREPLY` (0x10).
struct AccountLoginReply
{
    Header        h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const AccountLoginReply&) const = default;
};
static_assert(sizeof(AccountLoginReply) == 12);

/// `D2CS_BNETD_CHARLOGINREQ` (0x11) -- followed by NUL-terminated
/// character name and portrait.
struct CharLoginReq
{
    Header        h{};
    std::uint32_t sessionnum = 0;
    constexpr bool operator==(const CharLoginReq&) const = default;
};
static_assert(sizeof(CharLoginReq) == 12);

/// `BNETD_D2CS_CHARLOGINREPLY` (0x11).
struct CharLoginReply
{
    Header        h{};
    std::uint32_t reply = 0;
    constexpr bool operator==(const CharLoginReply&) const = default;
};
static_assert(sizeof(CharLoginReply) == 12);

/// `BNETD_D2CS_GAMEINFOREQ` (0x12) -- header followed by gamename.
struct GameInfoReq
{
    Header h{};
    constexpr bool operator==(const GameInfoReq&) const = default;
};
static_assert(sizeof(GameInfoReq) == 8);

/// `D2CS_BNETD_GAMEINFOREPLY` (0x12) -- followed by gamename.
/// Wire size = 9 (header + 1-byte difficulty); host `sizeof` will
/// generally be larger due to natural alignment. The codec writes
/// only the 9 wire bytes.
struct GameInfoReply
{
    Header       h{};
    std::uint8_t difficulty = 0;
    constexpr bool operator==(const GameInfoReply&) const = default;
};

static_assert(std::is_trivially_copyable_v<AuthReq>);
static_assert(std::is_trivially_copyable_v<AccountLoginReq>);
static_assert(std::is_trivially_copyable_v<GameInfoReply>);

/// Wire byte count (excluding trailing variable payload) for each
/// message body.
inline constexpr std::size_t kWireBytesAuthReq           = 12;
inline constexpr std::size_t kWireBytesAuthReplyFromD2cs = 12;
inline constexpr std::size_t kWireBytesAuthReplyFromBnetd= 12;
inline constexpr std::size_t kWireBytesAccountLoginReq   = 8 + 4 + 4 + 4 + 20;
inline constexpr std::size_t kWireBytesAccountLoginReply = 12;
inline constexpr std::size_t kWireBytesCharLoginReq      = 12;
inline constexpr std::size_t kWireBytesCharLoginReply    = 12;
inline constexpr std::size_t kWireBytesGameInfoReq       = 8;
inline constexpr std::size_t kWireBytesGameInfoReply     = 9;

}  // namespace pvpgn::protocol::d2cs::bnetd
