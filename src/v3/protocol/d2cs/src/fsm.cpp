// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/d2cs/fsm.hpp"

#include <algorithm>
#include <cstring>

namespace pvpgn::protocol::d2cs {

// ===========================================================================
// Construction
// ===========================================================================

D2CSSessionFsm::D2CSSessionFsm(D2CSFsmCallbacks callbacks)
    : callbacks_(std::move(callbacks))
{
}

// ===========================================================================
// Feed
// ===========================================================================

core::Result<size_t, core::Error> D2CSSessionFsm::feed(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return core::Result<size_t, core::Error>(static_cast<size_t>(0));
    }

    buffer_.insert(buffer_.end(), data, data + len);
    size_t consumed = 0;

    while (buffer_.size() >= kHeaderSize) {
        // Parse header: length (2 bytes LE) + type (1 byte)
        const uint16_t packet_len =
            static_cast<uint16_t>(buffer_[0]) |
            (static_cast<uint16_t>(buffer_[1]) << 8);

        if (packet_len < kHeaderSize) {
            return core::fail(
                core::make_error(core::StatusCode::InvalidArgument,
                                 "D2CS: packet length < 3"));
        }

        if (buffer_.size() < packet_len) {
            break;  // Wait for more data
        }

        const uint8_t  packet_type = buffer_[2];
        const uint8_t* payload     = buffer_.data() + kHeaderSize;
        const size_t   payload_len = packet_len - kHeaderSize;

        auto dispatch_result = dispatch(
            static_cast<D2CSPacketType>(packet_type), payload, payload_len);
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

core::Result<void, core::Error> D2CSSessionFsm::dispatch(
    D2CSPacketType type, const uint8_t* payload, size_t len)
{
    switch (type) {
        case D2CSPacketType::LOGINREQ:
            return handle_login(payload, len);
        case D2CSPacketType::CREATECHARREQ:
            return handle_create_char(payload, len);
        case D2CSPacketType::CREATEGAMEREQ:
            return handle_create_game(payload, len);
        case D2CSPacketType::JOINGAMEREQ:
            return handle_join_game(payload, len);
        case D2CSPacketType::GAMELISTREQ:
            return handle_game_list(payload, len);
        case D2CSPacketType::GAMEINFOREQ:
            return handle_game_info(payload, len);
        case D2CSPacketType::CHARLOGINREQ:
            return handle_char_login(payload, len);
        case D2CSPacketType::DELETECHARREQ:
            return handle_delete_char(payload, len);
        case D2CSPacketType::CHARLISTREQ:
        case D2CSPacketType::CHARLISTREQ110:
            return handle_char_list(payload, len);
        case D2CSPacketType::MOTDREQ:
            return handle_motd(payload, len);
        case D2CSPacketType::CANCELCREATEGAME:
            return handle_cancel_create_game(payload, len);
        case D2CSPacketType::CONVERTCHARREQ:
            return handle_convert_char(payload, len);
        case D2CSPacketType::LADDERREQ:
        case D2CSPacketType::CHARLADDERREQ:
            // Ladder requests are advisory; no callback defined — silently ignore.
            return core::Result<void, core::Error>();
        default:
            // Unknown packet type — log and ignore rather than killing the session.
            // This matches legacy behaviour in handle_d2cs.cpp where unrecognised
            // opcodes are silently dropped.
            return core::Result<void, core::Error>();
    }
}

// ===========================================================================
// Handlers
// ===========================================================================

core::Result<void, core::Error> D2CSSessionFsm::handle_login(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4..7]  uint32_t  session_key
    //   [8..]   char[]    account_name (null-terminated)
    //   [..]    char[]    char_name    (null-terminated)
    constexpr size_t kMinFixed = 8;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS LOGINREQ: payload too short"));
    }

    D2CSLoginRequest req;
    size_t offset = 0;

    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS LOGINREQ: cannot read seqno"));
    }
    if (!read_u32le(payload, len, offset, req.session_key)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS LOGINREQ: cannot read session_key"));
    }
    if (!read_cstring(payload, len, offset, req.account_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS LOGINREQ: unterminated account_name"));
    }
    if (!read_cstring(payload, len, offset, req.char_name)) {
        // char_name is optional in some client versions — treat as empty
        req.char_name.clear();
    }

    if (callbacks_.on_login) {
        auto result = callbacks_.on_login(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    state_ = D2CSSessionState::authenticating;
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_char_login(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]   uint32_t  seqno
    //   [4..7]   uint32_t  char_class
    //   [8..11]  uint32_t  char_level
    //   [12..15] uint32_t  char_status
    //   [16..]   char[]    account_name (null-terminated)
    //   [..]     char[]    char_name    (null-terminated)
    constexpr size_t kMinFixed = 16;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CHARLOGINREQ: payload too short"));
    }

    D2CSCharLoginRequest req;
    size_t offset = 0;

    if (!read_u32le(payload, len, offset, req.seqno))       goto short_payload;
    if (!read_u32le(payload, len, offset, req.char_class))  goto short_payload;
    if (!read_u32le(payload, len, offset, req.char_level))  goto short_payload;
    if (!read_u32le(payload, len, offset, req.char_status)) goto short_payload;

    if (!read_cstring(payload, len, offset, req.account_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CHARLOGINREQ: unterminated account_name"));
    }
    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CHARLOGINREQ: unterminated char_name"));
    }

    if (callbacks_.on_char_login) {
        auto result = callbacks_.on_char_login(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    state_ = D2CSSessionState::authenticated;
    return core::Result<void, core::Error>();

short_payload:
    return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                       "D2CS CHARLOGINREQ: payload too short"));
}

core::Result<void, core::Error> D2CSSessionFsm::handle_create_game(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4]     uint8_t   difficulty
    //   [5]     uint8_t   hardcore
    //   [6]     uint8_t   expansion
    //   [7..]   char[]    game_name        (null-terminated)
    //   [..]    char[]    game_password    (null-terminated)
    //   [..]    char[]    game_description (null-terminated)
    constexpr size_t kMinFixed = 7;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CREATEGAMEREQ: payload too short"));
    }

    D2CSCreateGameRequest req;
    size_t offset = 0;

    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CREATEGAMEREQ: cannot read seqno"));
    }
    req.difficulty = payload[offset++];
    req.hardcore   = payload[offset++];
    req.expansion  = payload[offset++];

    if (!read_cstring(payload, len, offset, req.game_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CREATEGAMEREQ: unterminated game_name"));
    }
    if (!read_cstring(payload, len, offset, req.game_password)) {
        req.game_password.clear();  // password is optional
    }
    if (!read_cstring(payload, len, offset, req.game_description)) {
        req.game_description.clear();  // description is optional
    }

    if (callbacks_.on_create_game) {
        auto result = callbacks_.on_create_game(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    state_ = D2CSSessionState::in_game;
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_join_game(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4..]   char[]    game_name     (null-terminated)
    //   [..]    char[]    game_password (null-terminated)
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS JOINGAMEREQ: payload too short"));
    }

    D2CSJoinGameRequest req;
    size_t offset = 0;

    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS JOINGAMEREQ: cannot read seqno"));
    }
    if (!read_cstring(payload, len, offset, req.game_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS JOINGAMEREQ: unterminated game_name"));
    }
    if (!read_cstring(payload, len, offset, req.game_password)) {
        req.game_password.clear();  // password is optional
    }

    if (callbacks_.on_join_game) {
        auto result = callbacks_.on_join_game(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    state_ = D2CSSessionState::in_game;
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_game_list(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4..7]  uint32_t  game_type
    constexpr size_t kMinFixed = 8;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS GAMELISTREQ: payload too short"));
    }

    D2CSGameListRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno))      goto short_payload;
    if (!read_u32le(payload, len, offset, req.game_type))  goto short_payload;

    if (callbacks_.on_game_list) {
        auto result = callbacks_.on_game_list(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();

short_payload:
    return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                       "D2CS GAMELISTREQ: payload too short"));
}

core::Result<void, core::Error> D2CSSessionFsm::handle_game_info(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4..]   char[]    game_name (null-terminated)
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS GAMEINFOREQ: payload too short"));
    }

    D2CSGameInfoRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS GAMEINFOREQ: cannot read seqno"));
    }
    if (!read_cstring(payload, len, offset, req.game_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS GAMEINFOREQ: unterminated game_name"));
    }

    if (callbacks_.on_game_info) {
        auto result = callbacks_.on_game_info(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_create_char(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4]     uint8_t   char_class
    //   [5]     uint8_t   char_flags
    //   [6..]   char[]    char_name (null-terminated)
    constexpr size_t kMinFixed = 6;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CREATECHARREQ: payload too short"));
    }

    D2CSCreateCharRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CREATECHARREQ: cannot read seqno"));
    }
    req.char_class = payload[offset++];
    req.char_flags = payload[offset++];

    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CREATECHARREQ: unterminated char_name"));
    }

    if (callbacks_.on_create_char) {
        auto result = callbacks_.on_create_char(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_delete_char(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4..]   char[]    char_name (null-terminated)
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS DELETECHARREQ: payload too short"));
    }

    D2CSDeleteCharRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS DELETECHARREQ: cannot read seqno"));
    }
    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS DELETECHARREQ: unterminated char_name"));
    }

    if (callbacks_.on_delete_char) {
        auto result = callbacks_.on_delete_char(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_char_list(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CHARLISTREQ: payload too short"));
    }

    D2CSCharListRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CHARLISTREQ: cannot read seqno"));
    }

    if (callbacks_.on_char_list) {
        auto result = callbacks_.on_char_list(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_motd(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS MOTDREQ: payload too short"));
    }

    D2CSMotdRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS MOTDREQ: cannot read seqno"));
    }

    if (callbacks_.on_motd) {
        auto result = callbacks_.on_motd(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_cancel_create_game(
    const uint8_t* /*payload*/, size_t /*len*/)
{
    // No payload required — just notify the application layer.
    if (callbacks_.on_cancel_create_game) {
        auto result = callbacks_.on_cancel_create_game();
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    // Revert to authenticated state (no longer in game queue)
    if (state_ == D2CSSessionState::in_game) {
        state_ = D2CSSessionState::authenticated;
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_convert_char(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //   [4..]   char[]    char_name (null-terminated)
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CONVERTCHARREQ: payload too short"));
    }

    D2CSConvertCharRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CONVERTCHARREQ: cannot read seqno"));
    }
    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CONVERTCHARREQ: unterminated char_name"));
    }

    if (callbacks_.on_convert_char) {
        auto result = callbacks_.on_convert_char(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

// ===========================================================================
// Packet builders
// ===========================================================================

std::vector<uint8_t> D2CSSessionFsm::make_login_reply(uint32_t result_code) {
    // Header(3) + result_code(4) = 7 bytes
    std::vector<uint8_t> v;
    v.reserve(7);
    push_header(v, 7, D2CSPacketType::LOGINREPLY);
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_char_login_reply(uint32_t result_code) {
    // Header(3) + result_code(4) = 7 bytes
    std::vector<uint8_t> v;
    v.reserve(7);
    push_header(v, 7, D2CSPacketType::CHARLOGINREPLY);
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_create_game_reply(
    uint32_t seqno, uint32_t game_id, uint32_t result_code)
{
    // Header(3) + seqno(4) + game_id(4) + u1(4) + result_code(4) = 19 bytes
    std::vector<uint8_t> v;
    v.reserve(19);
    push_header(v, 19, D2CSPacketType::CREATEGAMEREPLY);
    push_u32le(v, seqno);
    push_u32le(v, game_id);
    push_u32le(v, 0);           // u1 (reserved)
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_join_game_reply(
    uint32_t seqno, uint32_t game_id, uint32_t gs_ip,
    uint32_t token, uint32_t result_code)
{
    // Header(3) + seqno(4) + game_id(4) + u1(4) + gs_ip(4) + token(4) + result_code(4) = 27 bytes
    std::vector<uint8_t> v;
    v.reserve(27);
    push_header(v, 27, D2CSPacketType::JOINGAMEREPLY);
    push_u32le(v, seqno);
    push_u32le(v, game_id);
    push_u32le(v, 0);           // u1 (reserved)
    push_u32le(v, gs_ip);
    push_u32le(v, token);
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_char_list_reply(
    const std::vector<std::string>& char_names)
{
    // Header(3) + char_count(4) + names (each null-terminated)
    size_t names_len = 0;
    for (const auto& n : char_names) {
        names_len += n.size() + 1;  // +1 for null terminator
    }
    const uint16_t total = static_cast<uint16_t>(3 + 4 + names_len);

    std::vector<uint8_t> v;
    v.reserve(total);
    push_header(v, total, D2CSPacketType::CHARLISTREPLY);
    push_u32le(v, static_cast<uint32_t>(char_names.size()));
    for (const auto& n : char_names) {
        v.insert(v.end(), n.begin(), n.end());
        v.push_back(0x00);
    }
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_create_char_reply(uint32_t result_code) {
    // Header(3) + result_code(4) = 7 bytes
    std::vector<uint8_t> v;
    v.reserve(7);
    push_header(v, 7, D2CSPacketType::CREATECHARREPLY);
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_delete_char_reply(uint32_t result_code) {
    // Header(3) + result_code(4) = 7 bytes
    std::vector<uint8_t> v;
    v.reserve(7);
    push_header(v, 7, D2CSPacketType::DELETECHARREPLY);
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_motd_reply(std::string_view message) {
    // Header(3) + message (null-terminated)
    const uint16_t total = static_cast<uint16_t>(3 + message.size() + 1);
    std::vector<uint8_t> v;
    v.reserve(total);
    push_header(v, total, D2CSPacketType::MOTDREPLY);
    v.insert(v.end(), message.begin(), message.end());
    v.push_back(0x00);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_create_game_wait(uint32_t position) {
    // Header(3) + position(4) = 7 bytes
    std::vector<uint8_t> v;
    v.reserve(7);
    push_header(v, 7, D2CSPacketType::CREATEGAMEWAIT);
    push_u32le(v, position);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_convert_char_reply(uint32_t result_code) {
    // Header(3) + result_code(4) = 7 bytes
    std::vector<uint8_t> v;
    v.reserve(7);
    push_header(v, 7, D2CSPacketType::CONVERTCHARREPLY);
    push_u32le(v, result_code);
    return v;
}

// ===========================================================================
// Private helpers
// ===========================================================================

bool D2CSSessionFsm::read_cstring(
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

bool D2CSSessionFsm::read_u32le(
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

void D2CSSessionFsm::push_u32le(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void D2CSSessionFsm::push_header(
    std::vector<uint8_t>& v, uint16_t total_len, D2CSPacketType type)
{
    v.push_back(static_cast<uint8_t>(total_len & 0xFF));
    v.push_back(static_cast<uint8_t>((total_len >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>(type));
}

} // namespace pvpgn::protocol::d2cs
