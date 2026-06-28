// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure codec for the D2GS <-> D2DBS bridge protocol.
///
/// Header (8 bytes, all LE):
///   u16 size    -> total packet length, header included
///   u16 type    -> opcode
///   u32 seqno   -> correlation id
///
/// Implemented opcodes (matches `src/d2dbs/dbspacket.h`):
///   * 0x30 SAVE_DATA  (D2GS->D2DBS request, D2DBS->D2GS reply)
///   * 0x31 GET_DATA   (D2GS->D2DBS request, D2DBS->D2GS reply)
///   * 0x34 ECHO       (D2DBS->D2GS request, D2GS->D2DBS reply)
///
/// CONNECT (one-byte handshake, value 0x65) sits **outside** the
/// framed protocol: the very first byte the D2GS sends after TCP
/// accept is the connect-class byte. Modeled here as the standalone
/// `ConnectHandshake` value type with its own encode/decode helpers,
/// not part of either variant.
///
/// UPDATE_LADDER (0x32) and CHAR_LOCK (0x33) are inventoried in the
/// legacy header but not yet typed in v3.

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::d2dbs {

inline constexpr std::uint16_t kSaveData       = 0x30;
inline constexpr std::uint16_t kGetData        = 0x31;
inline constexpr std::uint16_t kUpdateLadder   = 0x32;
inline constexpr std::uint16_t kCharLock       = 0x33;
inline constexpr std::uint16_t kEcho           = 0x34;

inline constexpr std::uint8_t  kConnectClassD2gsToD2dbs = 0x65;

// SAVE_DATA result codes (legacy `D2DBS_SAVE_DATA_*`).
inline constexpr std::uint32_t kSaveDataSuccess = 0;
inline constexpr std::uint32_t kSaveDataFailed  = 1;

// GET_DATA result codes (legacy `D2DBS_GET_DATA_*`).
inline constexpr std::uint32_t kGetDataSuccess    = 0;
inline constexpr std::uint32_t kGetDataFailed     = 1;
inline constexpr std::uint32_t kGetDataCharLocked = 2;

// SAVE_DATA / GET_DATA datatype tags (legacy `D2GS_DATA_*`).
inline constexpr std::uint16_t kDataCharsave = 0x01;
inline constexpr std::uint16_t kDataPortrait = 0x02;

struct D2dbsHeader {
    std::uint16_t size  = 0;
    std::uint16_t type  = 0;
    std::uint32_t seqno = 0;
    static constexpr std::size_t kSize = 8;
    bool operator==(const D2dbsHeader&) const = default;
};

/// 1-byte handshake byte sent by D2GS immediately after TCP accept.
struct ConnectHandshake {
    std::uint8_t cclass = kConnectClassD2gsToD2dbs;
    bool operator==(const ConnectHandshake&) const = default;
};

/// 0x34 D2DBS -> D2GS: keepalive probe.
struct EchoRequest {
    std::uint32_t seqno = 0;
    bool operator==(const EchoRequest&) const = default;
};

/// 0x34 D2GS -> D2DBS: keepalive ack.
struct EchoReply {
    std::uint32_t seqno = 0;
    bool operator==(const EchoReply&) const = default;
};

/// 0x30 D2GS -> D2DBS: save char data / portrait.
struct SaveDataRequest {
    std::uint32_t              seqno    = 0;
    std::uint16_t              datatype = 0;  // kDataCharsave / kDataPortrait
    std::string                account;
    std::string                charname;
    std::string                realm;         // RealmName cstring (after charname)
    std::vector<std::uint8_t>  data;          // datalen == data.size()
    bool operator==(const SaveDataRequest&) const = default;
};

/// 0x30 D2DBS -> D2GS: ack for SAVE_DATA.
struct SaveDataReply {
    std::uint32_t  seqno    = 0;
    std::uint32_t  result   = 0;  // kSaveData{Success,Failed}
    std::uint16_t  datatype = 0;
    std::string    charname;
    bool operator==(const SaveDataReply&) const = default;
};

/// 0x31 D2GS -> D2DBS: fetch char data / portrait.
struct GetDataRequest {
    std::uint32_t  seqno    = 0;
    std::uint16_t  datatype = 0;
    std::string    account;
    std::string    charname;
    std::string    realm;    // RealmName cstring (after charname)
    bool operator==(const GetDataRequest&) const = default;
};

/// 0x31 D2DBS -> D2GS: response carrying the requested blob.
struct GetDataReply {
    std::uint32_t              seqno          = 0;
    std::uint32_t              result         = 0;  // kGetData{...}
    std::uint32_t              charcreatetime = 0;
    std::uint32_t              allowladder    = 0;
    std::uint16_t              datatype       = 0;
    std::string                charname;
    std::vector<std::uint8_t>  data;                // datalen == data.size()
    bool operator==(const GetDataReply&) const = default;
};

/// 0x32 D2GS -> D2DBS: ladder stat update for a character.
struct UpdateLadderRequest {
    std::uint32_t  seqno       = 0;
    std::uint32_t  charlevel   = 0;
    std::uint32_t  charexplow  = 0;
    std::uint32_t  charexphigh = 0;
    std::uint16_t  charclass   = 0;
    std::uint16_t  charstatus  = 0;
    std::string    charname;
    std::string    realmname;
    bool operator==(const UpdateLadderRequest&) const = default;
};

/// 0x33 D2GS -> D2DBS: lock/unlock a character.
struct CharLockRequest {
    std::uint32_t  seqno      = 0;
    /// Non-zero -> lock; zero -> unlock (matches legacy semantics).
    std::uint32_t  lockstatus = 0;
    /// Variable tail order on the wire is AccountName, CharName, RealmName
    /// (dbspacket.cpp dbs_packet_charlock reads all three, in that order).
    std::string    accountname;
    std::string    charname;
    std::string    realmname;
    bool operator==(const CharLockRequest&) const = default;
};

/// D2DBS -> D2GS direction (replies + echo probes).
using DownMessage = std::variant<EchoRequest, SaveDataReply, GetDataReply>;
/// D2GS -> D2DBS direction (requests + echo acks).
using UpMessage   = std::variant<EchoReply, SaveDataRequest, GetDataRequest,
                                 UpdateLadderRequest, CharLockRequest>;

core::Result<D2dbsHeader> parse_header(core::ByteView buf);
core::Result<DownMessage> decode_d2dbs_to_d2gs(core::ByteView buf);
core::Result<UpMessage>   decode_d2gs_to_d2dbs(core::ByteView buf);

/// Decode the 1-byte CONNECT handshake. Fails on empty buf or unknown
/// class byte.
core::Result<ConnectHandshake> decode_connect_handshake(core::ByteView buf);

core::Status<> encode(Writer& w, const ConnectHandshake& m);
core::Status<> encode(Writer& w, const EchoRequest&      m);
core::Status<> encode(Writer& w, const EchoReply&        m);
core::Status<> encode(Writer& w, const SaveDataRequest&  m);
core::Status<> encode(Writer& w, const SaveDataReply&    m);
core::Status<> encode(Writer& w, const GetDataRequest&   m);
core::Status<> encode(Writer& w, const GetDataReply&     m);
core::Status<> encode(Writer& w, const UpdateLadderRequest& m);
core::Status<> encode(Writer& w, const CharLockRequest&     m);

}  // namespace pvpgn::protocol::d2dbs
