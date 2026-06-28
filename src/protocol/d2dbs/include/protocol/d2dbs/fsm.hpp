// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fsm.hpp
/// D2DBS session finite-state machine (D2GS → D2DBS direction).
///
/// Packet type codes and wire layout are defined in `codec.hpp`
/// (which mirrors `src/d2dbs/dbspacket.h` in the original server).
///
/// The FSM owns a reassembly buffer and processes complete D2DBS packets
/// received from a D2GS (game server) connection.
/// All multi-byte integers are little-endian on the wire.
///
/// Header layout (8 bytes):
///   [0..1]  uint16_t  total packet length (including header)
///   [2..3]  uint16_t  packet type
///   [4..7]  uint32_t  sequence number
///
/// Payload follows immediately after the 8-byte header.

#include "core/result.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pvpgn::protocol::d2dbs {

// ---------------------------------------------------------------------------
// Packet type codes (D2GS → D2DBS, inbound)
// ---------------------------------------------------------------------------

enum class D2DBSPacketType : uint16_t {
    SAVE_DATA_REQUEST  = 0x30,  ///< D2GS requests D2DBS to save character data
    GET_DATA_REQUEST   = 0x31,  ///< D2GS requests D2DBS to load character data
    UPDATE_LADDER      = 0x32,  ///< D2GS sends updated ladder entry
    CHAR_LOCK          = 0x33,  ///< D2GS locks or unlocks a character
    ECHO_REPLY         = 0x34,  ///< D2GS replies to D2DBS keepalive echo
};

// ---------------------------------------------------------------------------
// Data type sub-codes (used in SAVE_DATA / GET_DATA)
// ---------------------------------------------------------------------------

enum class D2DBSDataType : uint16_t {
    CHAR_SAVE = 0x01,  ///< Character save file (.d2s binary blob)
    PORTRAIT  = 0x02,  ///< Character portrait / info file (.d2c binary blob)
};

// ---------------------------------------------------------------------------
// Request structs (parsed from wire)
// ---------------------------------------------------------------------------

/// Parsed SAVE_DATA_REQUEST (0x30).
/// Wire layout (after 8-byte header):
///   [0..1]  uint16_t  datatype   (D2DBSDataType)
///   [2..3]  uint16_t  datalen    (length of trailing data blob)
///   [4..]   char[]    account_name (null-terminated)
///   [..]    char[]    char_name    (null-terminated)
///   [..]    char[]    realm_name   (null-terminated)
///   [..]    uint8[]   data         (datalen bytes)
struct D2DBSCharSaveData {
    uint32_t             seqno;        ///< Sequence number from header
    D2DBSDataType        datatype;     ///< CHAR_SAVE or PORTRAIT
    std::string          account_name; ///< Account name
    std::string          char_name;    ///< Character name
    std::string          realm_name;   ///< Realm name
    std::vector<uint8_t> data;         ///< Raw save data blob
};

/// Parsed GET_DATA_REQUEST (0x31).
/// Wire layout (after 8-byte header):
///   [0..1]  uint16_t  datatype   (D2DBSDataType)
///   [2..]   char[]    account_name (null-terminated)
///   [..]    char[]    char_name    (null-terminated)
///   [..]    char[]    realm_name   (null-terminated)
struct D2DBSCharLoadData {
    uint32_t      seqno;        ///< Sequence number from header
    D2DBSDataType datatype;     ///< CHAR_SAVE or PORTRAIT
    std::string   account_name; ///< Account name
    std::string   char_name;    ///< Character name
    std::string   realm_name;   ///< Realm name
};

/// Parsed UPDATE_LADDER (0x32).
/// Wire layout (after 8-byte header):
///   [0..3]  uint32_t  charlevel    (character level)
///   [4..7]  uint32_t  charexplow   (experience low 32 bits)
///   [8..11] uint32_t  charexphigh  (experience high 32 bits)
///   [12..13] uint16_t charclass    (character class)
///   [14..15] uint16_t charstatus   (character status flags)
///   [16..]  char[]    char_name    (null-terminated)
///   [..]    char[]    realm_name   (null-terminated)
struct D2DBSCharLadderData {
    uint32_t    seqno;       ///< Sequence number from header
    uint32_t    charlevel;   ///< Character level
    uint32_t    charexplow;  ///< Experience (low 32 bits)
    uint32_t    charexphigh; ///< Experience (high 32 bits)
    uint16_t    charclass;   ///< Character class
    uint16_t    charstatus;  ///< Character status flags
    std::string char_name;   ///< Character name
    std::string realm_name;  ///< Realm name
};

/// Parsed CHAR_LOCK (0x33).
/// Wire layout (after 8-byte header):
///   [0..3]  uint32_t  lockstatus   (non-zero = lock, 0 = unlock)
///   [4..]   char[]    account_name (null-terminated)
///   [..]    char[]    char_name    (null-terminated)
///   [..]    char[]    realm_name   (null-terminated)
struct D2DBSCharLockReq {
    uint32_t    seqno;        ///< Sequence number from header
    uint32_t    lockstatus;   ///< Non-zero = lock, 0 = unlock
    std::string account_name; ///< Account name
    std::string char_name;    ///< Character name
    std::string realm_name;   ///< Realm name
};

/// Parsed ECHO_REPLY (0x34).
/// Wire layout (after 8-byte header): no payload.
struct D2DBSEchoReply {
    uint32_t seqno;  ///< Sequence number from header
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

/// Callbacks fired by the FSM when a complete packet is parsed.
/// All callbacks return `core::Result<void, core::Error>`.
/// Returning a failure causes `feed()` to propagate the error.
struct D2DBSFsmCallbacks {
    std::function<core::Result<void, core::Error>(const D2DBSCharSaveData&)>   on_char_save;
    std::function<core::Result<void, core::Error>(const D2DBSCharLoadData&)>   on_char_load;
    std::function<core::Result<void, core::Error>(const D2DBSCharLadderData&)> on_char_ladder;
    std::function<core::Result<void, core::Error>(const D2DBSCharLockReq&)>    on_char_lock;
    std::function<core::Result<void, core::Error>(const D2DBSEchoReply&)>      on_echo_reply;
    std::function<void()>                                                       on_disconnect;
};

// ---------------------------------------------------------------------------
// D2DBSSessionFsm
// ---------------------------------------------------------------------------

class D2DBSSessionFsm {
public:
    explicit D2DBSSessionFsm(D2DBSFsmCallbacks callbacks);

    /// Feed raw bytes from the TCP stream.
    /// Returns the number of bytes consumed (may be less than `len` if a
    /// partial packet is buffered).  Returns a failure if a packet is
    /// malformed or a callback returns an error.
    [[nodiscard]] core::Result<size_t, core::Error> feed(const uint8_t* data, size_t len);

    /// Reset the reassembly buffer and parser state.
    void reset();

    // -----------------------------------------------------------------------
    // Packet builders (D2DBS → D2GS outbound)
    // -----------------------------------------------------------------------

    /// Build a SAVE_DATA_REPLY packet.
    /// @param seqno     Sequence number echoed from request
    /// @param result    kSaveDataSuccess or kSaveDataFailed
    /// @param datatype  Data type echoed from request
    /// @param char_name Character name (null-terminated on wire)
    [[nodiscard]] static std::vector<uint8_t> make_save_data_reply(
        uint32_t seqno, uint32_t result, uint16_t datatype,
        const std::string& char_name);

    /// Build a GET_DATA_REPLY packet.
    /// @param seqno           Sequence number echoed from request
    /// @param result          kGetDataSuccess / kGetDataFailed / kGetDataCharLocked
    /// @param charcreatetime  Character creation timestamp
    /// @param allowladder     1 if eligible for ladder, 0 otherwise
    /// @param datatype        Data type echoed from request
    /// @param char_name       Character name (null-terminated on wire)
    /// @param data            Raw data blob (may be empty on failure)
    [[nodiscard]] static std::vector<uint8_t> make_get_data_reply(
        uint32_t seqno, uint32_t result,
        uint32_t charcreatetime, uint32_t allowladder,
        uint16_t datatype, const std::string& char_name,
        const std::vector<uint8_t>& data);

    /// Build an ECHO_REQUEST packet (keepalive ping to D2GS).
    /// @param seqno  Sequence number
    [[nodiscard]] static std::vector<uint8_t> make_echo_request(uint32_t seqno);

private:
    D2DBSFsmCallbacks    callbacks_;
    std::vector<uint8_t> buffer_;

    static constexpr size_t kHeaderSize    = 8;  ///< size(2) + type(2) + seqno(4)
    static constexpr size_t kMinPacketLen  = kHeaderSize;

    [[nodiscard]] core::Result<void, core::Error> dispatch(
        D2DBSPacketType type, uint32_t seqno,
        const uint8_t* payload, size_t len);

    [[nodiscard]] core::Result<void, core::Error> handle_save_data(
        uint32_t seqno, const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_get_data(
        uint32_t seqno, const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_update_ladder(
        uint32_t seqno, const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_char_lock(
        uint32_t seqno, const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_echo_reply(
        uint32_t seqno, const uint8_t* payload, size_t len);

    /// Read a null-terminated string from `buf[offset..]`.
    /// Advances `offset` past the null terminator.
    /// Returns false if no null terminator is found within `len` bytes.
    [[nodiscard]] static bool read_cstring(
        const uint8_t* buf, size_t len, size_t& offset, std::string& out);

    /// Read a little-endian uint32_t from `buf[offset..]`.
    /// Advances `offset` by 4.  Returns false if fewer than 4 bytes remain.
    [[nodiscard]] static bool read_u32le(
        const uint8_t* buf, size_t len, size_t& offset, uint32_t& out);

    /// Read a little-endian uint16_t from `buf[offset..]`.
    /// Advances `offset` by 2.  Returns false if fewer than 2 bytes remain.
    [[nodiscard]] static bool read_u16le(
        const uint8_t* buf, size_t len, size_t& offset, uint16_t& out);

    /// Read a single byte from `buf[offset..]`.
    /// Advances `offset` by 1.  Returns false if no byte remains.
    [[nodiscard]] static bool read_u8(
        const uint8_t* buf, size_t len, size_t& offset, uint8_t& out);

    /// Write a little-endian uint32_t into a vector.
    static void push_u32le(std::vector<uint8_t>& v, uint32_t val);

    /// Write a little-endian uint16_t into a vector.
    static void push_u16le(std::vector<uint8_t>& v, uint16_t val);

    /// Write an 8-byte packet header (size LE + type LE + seqno LE) into a vector.
    static void push_header(std::vector<uint8_t>& v, uint16_t total_len,
                            D2DBSPacketType type, uint32_t seqno);
};

} // namespace pvpgn::protocol::d2dbs
