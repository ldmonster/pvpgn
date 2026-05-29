// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/d2dbs/fsm.hpp"

#include <algorithm>
#include <cstring>

namespace pvpgn::protocol::d2dbs {

// ===========================================================================
// Construction
// ===========================================================================

D2DBSSessionFsm::D2DBSSessionFsm(D2DBSFsmCallbacks callbacks)
    : callbacks_(std::move(callbacks))
{
}

// ===========================================================================
// Reset
// ===========================================================================

void D2DBSSessionFsm::reset() {
    buffer_.clear();
}

// ===========================================================================
// Feed
// ===========================================================================

core::Result<size_t, core::Error> D2DBSSessionFsm::feed(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return core::Result<size_t, core::Error>(static_cast<size_t>(0));
    }

    buffer_.insert(buffer_.end(), data, data + len);
    size_t consumed = 0;

    while (buffer_.size() >= kHeaderSize) {
        // Parse header: size(2 LE) + type(2 LE) + seqno(4 LE)
        const uint16_t packet_len =
            static_cast<uint16_t>(buffer_[0]) |
            (static_cast<uint16_t>(buffer_[1]) << 8);

        if (packet_len < kHeaderSize) {
            return core::fail(
                core::make_error(core::StatusCode::InvalidArgument,
                                 "D2DBS: packet length < 8"));
        }

        if (buffer_.size() < packet_len) {
            break;  // Wait for more data
        }

        const uint16_t packet_type =
            static_cast<uint16_t>(buffer_[2]) |
            (static_cast<uint16_t>(buffer_[3]) << 8);

        const uint32_t seqno =
            static_cast<uint32_t>(buffer_[4])        |
            (static_cast<uint32_t>(buffer_[5]) << 8)  |
            (static_cast<uint32_t>(buffer_[6]) << 16) |
            (static_cast<uint32_t>(buffer_[7]) << 24);

        const uint8_t* payload     = buffer_.data() + kHeaderSize;
        const size_t   payload_len = packet_len - kHeaderSize;

        auto dispatch_result = dispatch(
            static_cast<D2DBSPacketType>(packet_type), seqno, payload, payload_len);
        if (!dispatch_result) {
            return core::fail(std::move(dispatch_result).error());
        }

        buffer_.erase(buffer_.begin(), buffer_.begin() + packet_len);
        consumed += packet_len;
    }

    return core::Result<size_t, core::Error>(consumed);
}

// ===========================================================================
// Dispatch
// ===========================================================================

core::Result<void, core::Error> D2DBSSessionFsm::dispatch(
    D2DBSPacketType type, uint32_t seqno,
    const uint8_t* payload, size_t len)
{
    switch (type) {
        case D2DBSPacketType::SAVE_DATA_REQUEST:
            return handle_save_data(seqno, payload, len);
        case D2DBSPacketType::GET_DATA_REQUEST:
            return handle_get_data(seqno, payload, len);
        case D2DBSPacketType::UPDATE_LADDER:
            return handle_update_ladder(seqno, payload, len);
        case D2DBSPacketType::CHAR_LOCK:
            return handle_char_lock(seqno, payload, len);
        case D2DBSPacketType::ECHO_REPLY:
            return handle_echo_reply(seqno, payload, len);
        default:
            // Unknown packet type — log and ignore rather than killing the session.
            // This matches legacy behaviour in dbspacket.cpp where unrecognised
            // opcodes are logged and the connection is dropped; here we silently
            // ignore to be more resilient.
            return core::Result<void, core::Error>();
    }
}

// ===========================================================================
// Handlers
// ===========================================================================

core::Result<void, core::Error> D2DBSSessionFsm::handle_save_data(
    uint32_t seqno, const uint8_t* payload, size_t len)
{
    // Wire layout (after 8-byte header):
    //   [0..1]  uint16_t  datatype   (D2DBSDataType)
    //   [2..3]  uint16_t  datalen    (length of trailing data blob)
    //   [4..]   char[]    account_name (null-terminated)
    //   [..]    char[]    char_name    (null-terminated)
    //   [..]    char[]    realm_name   (null-terminated)
    //   [..]    uint8[]   data         (datalen bytes)
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2DBS SAVE_DATA_REQUEST: payload too short"));
    }

    D2DBSCharSaveData req;
    req.seqno = seqno;
    size_t offset = 0;

    uint16_t datatype_raw = 0;
    uint16_t datalen      = 0;
    if (!read_u16le(payload, len, offset, datatype_raw)) goto short_payload;
    if (!read_u16le(payload, len, offset, datalen))      goto short_payload;

    req.datatype = static_cast<D2DBSDataType>(datatype_raw);

    if (!read_cstring(payload, len, offset, req.account_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS SAVE_DATA_REQUEST: unterminated account_name"));
    }
    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS SAVE_DATA_REQUEST: unterminated char_name"));
    }
    if (!read_cstring(payload, len, offset, req.realm_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS SAVE_DATA_REQUEST: unterminated realm_name"));
    }

    if (offset + datalen > len) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS SAVE_DATA_REQUEST: data blob truncated"));
    }
    req.data.assign(payload + offset, payload + offset + datalen);

    if (callbacks_.on_char_save) {
        auto result = callbacks_.on_char_save(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();

short_payload:
    return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                       "D2DBS SAVE_DATA_REQUEST: payload too short"));
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_get_data(
    uint32_t seqno, const uint8_t* payload, size_t len)
{
    // Wire layout (after 8-byte header):
    //   [0..1]  uint16_t  datatype   (D2DBSDataType)
    //   [2..]   char[]    account_name (null-terminated)
    //   [..]    char[]    char_name    (null-terminated)
    //   [..]    char[]    realm_name   (null-terminated)
    constexpr size_t kMinFixed = 2;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2DBS GET_DATA_REQUEST: payload too short"));
    }

    D2DBSCharLoadData req;
    req.seqno = seqno;
    size_t offset = 0;

    uint16_t datatype_raw = 0;
    if (!read_u16le(payload, len, offset, datatype_raw)) goto short_payload;

    req.datatype = static_cast<D2DBSDataType>(datatype_raw);

    if (!read_cstring(payload, len, offset, req.account_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS GET_DATA_REQUEST: unterminated account_name"));
    }
    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS GET_DATA_REQUEST: unterminated char_name"));
    }
    if (!read_cstring(payload, len, offset, req.realm_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS GET_DATA_REQUEST: unterminated realm_name"));
    }

    if (callbacks_.on_char_load) {
        auto result = callbacks_.on_char_load(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();

short_payload:
    return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                       "D2DBS GET_DATA_REQUEST: payload too short"));
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_update_ladder(
    uint32_t seqno, const uint8_t* payload, size_t len)
{
    // Wire layout (after 8-byte header):
    //   [0..3]   uint32_t  charlevel
    //   [4..7]   uint32_t  charexplow
    //   [8..11]  uint32_t  charexphigh
    //   [12..13] uint16_t  charclass
    //   [14..15] uint16_t  charstatus
    //   [16..]   char[]    char_name  (null-terminated)
    //   [..]     char[]    realm_name (null-terminated)
    constexpr size_t kMinFixed = 16;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2DBS UPDATE_LADDER: payload too short"));
    }

    D2DBSCharLadderData req;
    req.seqno = seqno;
    size_t offset = 0;

    if (!read_u32le(payload, len, offset, req.charlevel))   goto short_payload;
    if (!read_u32le(payload, len, offset, req.charexplow))  goto short_payload;
    if (!read_u32le(payload, len, offset, req.charexphigh)) goto short_payload;
    if (!read_u16le(payload, len, offset, req.charclass))   goto short_payload;
    if (!read_u16le(payload, len, offset, req.charstatus))  goto short_payload;

    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS UPDATE_LADDER: unterminated char_name"));
    }
    if (!read_cstring(payload, len, offset, req.realm_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS UPDATE_LADDER: unterminated realm_name"));
    }

    if (callbacks_.on_char_ladder) {
        auto result = callbacks_.on_char_ladder(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();

short_payload:
    return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                       "D2DBS UPDATE_LADDER: payload too short"));
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_char_lock(
    uint32_t seqno, const uint8_t* payload, size_t len)
{
    // Wire layout (after 8-byte header):
    //   [0..3]  uint32_t  lockstatus   (non-zero = lock, 0 = unlock)
    //   [4..]   char[]    account_name (null-terminated)
    //   [..]    char[]    char_name    (null-terminated)
    //   [..]    char[]    realm_name   (null-terminated)
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2DBS CHAR_LOCK: payload too short"));
    }

    D2DBSCharLockReq req;
    req.seqno = seqno;
    size_t offset = 0;

    if (!read_u32le(payload, len, offset, req.lockstatus)) goto short_payload;

    if (!read_cstring(payload, len, offset, req.account_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS CHAR_LOCK: unterminated account_name"));
    }
    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS CHAR_LOCK: unterminated char_name"));
    }
    if (!read_cstring(payload, len, offset, req.realm_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2DBS CHAR_LOCK: unterminated realm_name"));
    }

    if (callbacks_.on_char_lock) {
        auto result = callbacks_.on_char_lock(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();

short_payload:
    return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                       "D2DBS CHAR_LOCK: payload too short"));
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_echo_reply(
    uint32_t seqno, const uint8_t* /*payload*/, size_t /*len*/)
{
    // No payload — just notify the application layer.
    D2DBSEchoReply req;
    req.seqno = seqno;

    if (callbacks_.on_echo_reply) {
        auto result = callbacks_.on_echo_reply(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

// ===========================================================================
// Packet builders
// ===========================================================================

std::vector<uint8_t> D2DBSSessionFsm::make_save_data_reply(
    uint32_t seqno, uint32_t result, uint16_t datatype,
    const std::string& char_name)
{
    // Header(8) + result(4) + datatype(2) + char_name(N+1) bytes
    const uint16_t total = static_cast<uint16_t>(
        8 + 4 + 2 + char_name.size() + 1);
    std::vector<uint8_t> v;
    v.reserve(total);
    push_header(v, total, D2DBSPacketType::SAVE_DATA_REQUEST, seqno);
    push_u32le(v, result);
    push_u16le(v, datatype);
    v.insert(v.end(), char_name.begin(), char_name.end());
    v.push_back(0x00);
    return v;
}

std::vector<uint8_t> D2DBSSessionFsm::make_get_data_reply(
    uint32_t seqno, uint32_t result,
    uint32_t charcreatetime, uint32_t allowladder,
    uint16_t datatype, const std::string& char_name,
    const std::vector<uint8_t>& data)
{
    // Header(8) + result(4) + charcreatetime(4) + allowladder(4)
    //           + datatype(2) + datalen(2) + char_name(N+1) + data(M)
    const uint16_t datalen = static_cast<uint16_t>(data.size());
    const uint16_t total   = static_cast<uint16_t>(
        8 + 4 + 4 + 4 + 2 + 2 + char_name.size() + 1 + datalen);
    std::vector<uint8_t> v;
    v.reserve(total);
    push_header(v, total, D2DBSPacketType::GET_DATA_REQUEST, seqno);
    push_u32le(v, result);
    push_u32le(v, charcreatetime);
    push_u32le(v, allowladder);
    push_u16le(v, datatype);
    push_u16le(v, datalen);
    v.insert(v.end(), char_name.begin(), char_name.end());
    v.push_back(0x00);
    v.insert(v.end(), data.begin(), data.end());
    return v;
}

std::vector<uint8_t> D2DBSSessionFsm::make_echo_request(uint32_t seqno) {
    // Header only — no payload
    std::vector<uint8_t> v;
    v.reserve(8);
    push_header(v, 8, D2DBSPacketType::ECHO_REPLY, seqno);
    return v;
}

// ===========================================================================
// Private helpers
// ===========================================================================

bool D2DBSSessionFsm::read_cstring(
    const uint8_t* buf, size_t len, size_t& offset, std::string& out)
{
    const size_t start = offset;
    while (offset < len && buf[offset] != 0x00) {
        ++offset;
    }
    if (offset >= len) {
        // No null terminator found
        return false;
    }
    out.assign(reinterpret_cast<const char*>(buf + start), offset - start);
    ++offset;  // skip null terminator
    return true;
}

bool D2DBSSessionFsm::read_u32le(
    const uint8_t* buf, size_t len, size_t& offset, uint32_t& out)
{
    if (offset + 4 > len) {
        return false;
    }
    out = static_cast<uint32_t>(buf[offset])
        | (static_cast<uint32_t>(buf[offset + 1]) << 8)
        | (static_cast<uint32_t>(buf[offset + 2]) << 16)
        | (static_cast<uint32_t>(buf[offset + 3]) << 24);
    offset += 4;
    return true;
}

bool D2DBSSessionFsm::read_u16le(
    const uint8_t* buf, size_t len, size_t& offset, uint16_t& out)
{
    if (offset + 2 > len) {
        return false;
    }
    out = static_cast<uint16_t>(buf[offset])
        | (static_cast<uint16_t>(buf[offset + 1]) << 8);
    offset += 2;
    return true;
}

bool D2DBSSessionFsm::read_u8(
    const uint8_t* buf, size_t len, size_t& offset, uint8_t& out)
{
    if (offset + 1 > len) {
        return false;
    }
    out = buf[offset];
    offset += 1;
    return true;
}

void D2DBSSessionFsm::push_u32le(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void D2DBSSessionFsm::push_u16le(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void D2DBSSessionFsm::push_header(
    std::vector<uint8_t>& v, uint16_t total_len,
    D2DBSPacketType type, uint32_t seqno)
{
    push_u16le(v, total_len);
    push_u16le(v, static_cast<uint16_t>(type));
    push_u32le(v, seqno);
}

} // namespace pvpgn::protocol::d2dbs
