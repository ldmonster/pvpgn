// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// D2GS <-> D2DBS wire types, mirrored from `src/d2dbs/dbspacket.h`.
///
/// All multi-byte fields use native fixed-width integers; the codec
/// layer is responsible for LE byte-order conversion.
///
/// Header layout (8 bytes, all LE):
///   [0..1]  uint16_t  total packet length (including header)
///   [2..3]  uint16_t  packet type
///   [4..7]  uint32_t  sequence number
///
/// Variable-length trailing data (account names, char names, realm names,
/// save blobs) is NOT represented in these structs and must be handled by
/// the codec / FSM.

#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::d2dbs::wire {

// ---- Message type codes (D2GS → D2DBS, inbound to D2DBS) ---------------

/// Save character data (charsave or portrait/charinfo).
/// Wire: [header:8][datatype:2][datalen:2][AccountName:cstr][CharName:cstr][RealmName:cstr][data:datalen]
inline constexpr std::uint16_t kSaveDataRequest  = 0x30;

/// Load (get) character data.
/// Wire: [header:8][datatype:2][AccountName:cstr][CharName:cstr][RealmName:cstr]
inline constexpr std::uint16_t kGetDataRequest   = 0x31;

/// Update ladder entry for a character.
/// Wire: [header:8][charlevel:4][charexplow:4][charexphigh:4][charclass:2][charstatus:2][CharName:cstr][RealmName:cstr]
inline constexpr std::uint16_t kUpdateLadder     = 0x32;

/// Lock or unlock a character (lockstatus != 0 → lock, 0 → unlock).
/// Wire: [header:8][lockstatus:4][AccountName:cstr][CharName:cstr][RealmName:cstr]
inline constexpr std::uint16_t kCharLock         = 0x33;

/// Echo reply from D2GS (response to D2DBS keepalive echo request).
/// Wire: [header:8]  (no payload)
inline constexpr std::uint16_t kEchoReply        = 0x34;

// ---- Message type codes (D2DBS → D2GS, outbound from D2DBS) -------------

/// Reply to save data request.
/// Wire: [header:8][result:4][datatype:2][CharName:cstr]
inline constexpr std::uint16_t kSaveDataReply    = 0x30;

/// Reply to get data request.
/// Wire: [header:8][result:4][charcreatetime:4][allowladder:4][datatype:2][datalen:2][CharName:cstr][data:datalen]
inline constexpr std::uint16_t kGetDataReply     = 0x31;

/// Echo request sent by D2DBS to D2GS (keepalive).
/// Wire: [header:8]  (no payload)
inline constexpr std::uint16_t kEchoRequest      = 0x34;

// ---- Connect handshake (one-byte, before framed protocol) ---------------

/// First byte sent by D2GS after TCP connect.
inline constexpr std::uint8_t kConnectClassD2gsToD2dbs = 0x65;

// ---- Data type sub-codes (used in SAVE_DATA / GET_DATA) -----------------

/// Character save file (.d2s binary blob).
inline constexpr std::uint16_t kDataTypeCharSave    = 0x01;

/// Character portrait / info file (.d2c binary blob).
inline constexpr std::uint16_t kDataTypePortrait    = 0x02;

// ---- Result codes -------------------------------------------------------

inline constexpr std::uint32_t kSaveDataSuccess     = 0;
inline constexpr std::uint32_t kSaveDataFailed      = 1;

inline constexpr std::uint32_t kGetDataSuccess      = 0;
inline constexpr std::uint32_t kGetDataFailed       = 1;
inline constexpr std::uint32_t kGetDataCharLocked   = 2;

// ---- Header -------------------------------------------------------------

/// 8-byte framing header shared by all D2GS↔D2DBS packets.
struct PacketHeader {
    std::uint16_t size  = 0;  ///< Total packet length including this header
    std::uint16_t type  = 0;  ///< Packet type code
    std::uint32_t seqno = 0;  ///< Sequence / correlation number
    constexpr bool operator==(const PacketHeader&) const = default;
};
inline constexpr std::size_t kWireBytesPacketHeader = 8;
static_assert(std::is_trivially_copyable_v<PacketHeader>);

// ---- Fixed-part structs (header + fixed fields; variable tail omitted) --

/// D2GS → D2DBS: save character data.
struct SaveDataRequest {
    PacketHeader   h{};
    std::uint16_t  datatype = 0;  ///< kDataTypeCharSave or kDataTypePortrait
    std::uint16_t  datalen  = 0;  ///< Length of trailing data blob
    // Variable tail: AccountName\0 CharName\0 RealmName\0 data[datalen]
    constexpr bool operator==(const SaveDataRequest&) const = default;
};
inline constexpr std::size_t kWireBytesSaveDataRequest = kWireBytesPacketHeader + 4;
static_assert(std::is_trivially_copyable_v<SaveDataRequest>);

/// D2DBS → D2GS: reply to save data request.
struct SaveDataReply {
    PacketHeader   h{};
    std::uint32_t  result   = 0;  ///< kSaveDataSuccess / kSaveDataFailed
    std::uint16_t  datatype = 0;  ///< Echoed from request
    // Variable tail: CharName\0
    constexpr bool operator==(const SaveDataReply&) const = default;
};
inline constexpr std::size_t kWireBytesSaveDataReply = kWireBytesPacketHeader + 6;
static_assert(std::is_trivially_copyable_v<SaveDataReply>);

/// D2GS → D2DBS: load (get) character data.
struct GetDataRequest {
    PacketHeader   h{};
    std::uint16_t  datatype = 0;  ///< kDataTypeCharSave or kDataTypePortrait
    // Variable tail: AccountName\0 CharName\0 RealmName\0
    constexpr bool operator==(const GetDataRequest&) const = default;
};
inline constexpr std::size_t kWireBytesGetDataRequest = kWireBytesPacketHeader + 2;
static_assert(std::is_trivially_copyable_v<GetDataRequest>);

/// D2DBS → D2GS: reply to get data request.
struct GetDataReply {
    PacketHeader   h{};
    std::uint32_t  result          = 0;  ///< kGetDataSuccess / kGetDataFailed / kGetDataCharLocked
    std::uint32_t  charcreatetime  = 0;  ///< Character creation timestamp
    std::uint32_t  allowladder     = 0;  ///< 1 if character is eligible for ladder
    std::uint16_t  datatype        = 0;  ///< Echoed from request
    std::uint16_t  datalen         = 0;  ///< Length of trailing data blob
    // Variable tail: CharName\0 data[datalen]
    constexpr bool operator==(const GetDataReply&) const = default;
};
inline constexpr std::size_t kWireBytesGetDataReply = kWireBytesPacketHeader + 16;
static_assert(std::is_trivially_copyable_v<GetDataReply>);

/// D2GS → D2DBS: update ladder entry.
struct UpdateLadder {
    PacketHeader   h{};
    std::uint32_t  charlevel   = 0;  ///< Character level
    std::uint32_t  charexplow  = 0;  ///< Experience (low 32 bits)
    std::uint32_t  charexphigh = 0;  ///< Experience (high 32 bits)
    std::uint16_t  charclass   = 0;  ///< Character class
    std::uint16_t  charstatus  = 0;  ///< Character status flags
    // Variable tail: CharName\0 RealmName\0
    constexpr bool operator==(const UpdateLadder&) const = default;
};
inline constexpr std::size_t kWireBytesUpdateLadder = kWireBytesPacketHeader + 16;
static_assert(std::is_trivially_copyable_v<UpdateLadder>);

/// D2GS → D2DBS: lock or unlock a character.
struct CharLock {
    PacketHeader   h{};
    std::uint32_t  lockstatus = 0;  ///< Non-zero = lock, 0 = unlock
    // Variable tail: AccountName\0 CharName\0 RealmName\0
    constexpr bool operator==(const CharLock&) const = default;
};
inline constexpr std::size_t kWireBytesCharLock = kWireBytesPacketHeader + 4;
static_assert(std::is_trivially_copyable_v<CharLock>);

/// D2DBS → D2GS: echo request (keepalive ping).
struct EchoRequest {
    PacketHeader h{};
    constexpr bool operator==(const EchoRequest&) const = default;
};
inline constexpr std::size_t kWireBytesEchoRequest = kWireBytesPacketHeader;
static_assert(std::is_trivially_copyable_v<EchoRequest>);

/// D2GS → D2DBS: echo reply (keepalive pong).
struct EchoReply {
    PacketHeader h{};
    constexpr bool operator==(const EchoReply&) const = default;
};
inline constexpr std::size_t kWireBytesEchoReply = kWireBytesPacketHeader;
static_assert(std::is_trivially_copyable_v<EchoReply>);

} // namespace pvpgn::protocol::d2dbs::wire
