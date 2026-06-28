// SPDX-License-Identifier: GPL-2.0-or-later
/// @file d2cs_handlers.cpp
/// D2CSSessionFsm — packet handler methods.
///
/// Each handler parses the raw payload bytes into a typed request struct
/// and fires the corresponding application-layer callback.  State
/// transitions (authenticating → authenticated → in_game) are applied here.
///
///   handle_login()              — LOGINREQ (0x01)
///   handle_char_login()         — CHARLOGINREQ (0x07)
///   handle_create_game()        — CREATEGAMEREQ (0x03)
///   handle_join_game()          — JOINGAMEREQ (0x04)
///   handle_game_list()          — GAMELISTREQ (0x05)
///   handle_game_info()          — GAMEINFOREQ (0x06)
///   handle_create_char()        — CREATECHARREQ (0x02)
///   handle_delete_char()        — DELETECHARREQ (0x0A)
///   handle_char_list()          — CHARLISTREQ (0x17)
///   handle_char_list_110()      — CHARLISTREQ110 (0x19)
///   handle_motd()               — MOTDREQ (0x12)
///   handle_cancel_create_game() — CANCELCREATEGAME (0x0C)
///   handle_convert_char()       — CONVERTCHARREQ (0x19)
///   handle_ladder()             — LADDERREQ (0x1A)
///   handle_char_ladder()        — CHARLADDERREQ (0x1B)

#include "protocol/d2cs/fsm.hpp"

namespace pvpgn::protocol::d2cs {

core::Result<void, core::Error> D2CSSessionFsm::handle_login(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after the 3-byte header), per d2cs_protocol.h
    // t_client_d2cs_loginreq — a fixed 64-byte block then the account name:
    //   [0]  u32 seqno      [4]  u32 u1          [8]  u32 bncs_addr1
    //   [12] u32 sessionnum [16] u32 sessionkey  [20] u32 cdkey_id
    //   [24] u32 u5         [28] u32 clienttag   [32] u32 bnversion
    //   [36] u32 bncs_addr2 [40] u32 u6          [44] u32 secret_hash[5]
    //   [64] account_name (null-terminated)
    // (The earlier 8-byte seqno+session_key layout was a fabrication that
    //  misread a real client's account name from the middle of the fixed block.)
    constexpr size_t kFixed = 64;
    if (len < kFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS LOGINREQ: payload too short"));
    }

    D2CSLoginRequest req;
    size_t offset = 0;
    // len >= 64 guarantees these 16 little-endian reads stay in bounds.
    auto rd32 = [&]() -> uint32_t {
        const uint32_t v =
            static_cast<uint32_t>(payload[offset]) |
            (static_cast<uint32_t>(payload[offset + 1]) << 8) |
            (static_cast<uint32_t>(payload[offset + 2]) << 16) |
            (static_cast<uint32_t>(payload[offset + 3]) << 24);
        offset += 4;
        return v;
    };
    req.seqno = rd32();
    rd32();  // u1
    rd32();  // bncs_addr1
    req.sessionnum  = rd32();
    req.session_key = rd32();
    rd32();  // cdkey_id
    rd32();  // u5
    rd32();  // clienttag
    rd32();  // bnversion
    rd32();  // bncs_addr2
    rd32();  // u6
    for (auto& word : req.secret_hash) word = rd32();

    if (!read_cstring(payload, len, offset, req.account_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS LOGINREQ: unterminated account_name"));
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
    // Wire layout (after 3-byte header) per d2cs_protocol.h
    // t_client_d2cs_charloginreq:
    //   [0..]   char[]    char_name (null-terminated)
    // There are no fixed fields: the account comes from the session (set at
    // LOGINREQ) and class/level/status come from the server-side charinfo, not
    // the client.
    D2CSCharLoginRequest req;
    size_t offset = 0;

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
    // Wire layout (after 3-byte header), per d2cs_protocol.h
    // t_client_d2cs_gamelistreq: bn_short seqno + bn_int gameflag.
    //   [0..1]  uint16_t  seqno
    //   [2..5]  uint32_t  game_type (gameflag)
    // (The codec decoder reads u16 seqno too; the handler previously read u32,
    // misframing the gameflag for a real client.)
    constexpr size_t kMinFixed = 6;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS GAMELISTREQ: payload too short"));
    }

    D2CSGameListRequest req;
    size_t offset = 0;
    std::uint16_t seqno16 = 0;
    if (!read_u16le(payload, len, offset, seqno16))        goto short_payload;
    req.seqno = seqno16;
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
    // Wire layout (after 3-byte header), per d2cs_protocol.h
    // t_client_d2cs_gameinforeq: bn_short seqno + game_name cstring.
    //   [0..1]  uint16_t  seqno
    //   [2..]   char[]    game_name (null-terminated)
    // (The codec decoder reads u16 seqno; the handler previously read u32.)
    constexpr size_t kMinFixed = 2;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS GAMEINFOREQ: payload too short"));
    }

    D2CSGameInfoRequest req;
    size_t offset = 0;
    std::uint16_t seqno16 = 0;
    if (!read_u16le(payload, len, offset, seqno16)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS GAMEINFOREQ: cannot read seqno"));
    }
    req.seqno = seqno16;
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
    // Wire layout (after 3-byte header) per d2cs_protocol.h
    // t_client_d2cs_createcharreq:
    //   [0..1]  uint16_t  chclass (character class)
    //   [2..3]  uint16_t  u1      (always zero)
    //   [4..5]  uint16_t  status  (same as in .d2s file)
    //   [6..]   char[]    char_name (null-terminated)
    // There is NO seqno, and class/status are 16-bit fields.
    constexpr size_t kMinFixed = 6;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CREATECHARREQ: payload too short"));
    }

    D2CSCreateCharRequest req;
    size_t offset = 0;
    uint16_t u1 = 0;
    if (!read_u16le(payload, len, offset, req.char_class) ||
        !read_u16le(payload, len, offset, u1) ||
        !read_u16le(payload, len, offset, req.char_status)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CREATECHARREQ: payload too short"));
    }
    (void)u1;  // always zero on the wire

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
    // Wire layout (after 3-byte header) per d2cs_protocol.h
    // t_client_d2cs_deletecharreq:
    //   [0..1]  uint16_t  u1 (always zero)
    //   [2..]   char[]    char_name (null-terminated)
    // There is NO seqno; only a 2-byte u1 precedes the name.
    constexpr size_t kMinFixed = 2;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS DELETECHARREQ: payload too short"));
    }

    D2CSDeleteCharRequest req;
    size_t offset = 0;
    uint16_t u1 = 0;
    if (!read_u16le(payload, len, offset, u1)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS DELETECHARREQ: payload too short"));
    }
    (void)u1;  // always zero on the wire
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

core::Result<void, core::Error> D2CSSessionFsm::handle_char_list_110(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  seqno
    //
    // This is the 1.10+ variant of CHARLISTREQ.  The wire format is identical
    // to CHARLISTREQ (0x17) in the v3 redesign; the distinction is surfaced to
    // the application layer via the separate on_char_list_110 callback so that
    // the reply can include per-character expire_time fields if needed.
    constexpr size_t kMinFixed = 4;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CHARLISTREQ110: payload too short"));
    }

    D2CSCharListRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.seqno)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CHARLISTREQ110: cannot read seqno"));
    }

    // Fire the base on_char_list callback first (backward-compat: TC-34).
    if (callbacks_.on_char_list) {
        auto result = callbacks_.on_char_list(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    // Fire the 1.10+-specific callback if registered.
    if (callbacks_.on_char_list_110) {
        auto result = callbacks_.on_char_list_110(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_motd(
    const uint8_t* payload, size_t len)
{
    // The real CLIENT_D2CS_MOTDREQ (t_client_d2cs_motdreq) is just the 3-byte
    // header with NO body — the previous 4-byte seqno requirement rejected a
    // real client's request. An optional leading u32 is tolerated for forward
    // compatibility but not required.
    D2CSMotdRequest req;
    size_t offset = 0;
    if (len >= 4) {
        (void)read_u32le(payload, len, offset, req.seqno);
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

core::Result<void, core::Error> D2CSSessionFsm::handle_ladder(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0]     uint8_t   ladder_type  (0=standard, 1=hardcore, etc.)
    //   [1..2]  uint16_t  start_pos    (start position in ladder, LE)
    constexpr size_t kMinFixed = 3;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS LADDERREQ: payload too short"));
    }

    D2CSLadderRequest req;
    size_t offset = 0;
    if (!read_u8(payload, len, offset, req.ladder_type)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS LADDERREQ: cannot read ladder_type"));
    }
    if (!read_u16le(payload, len, offset, req.start_pos)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS LADDERREQ: cannot read start_pos"));
    }

    if (callbacks_.on_ladder) {
        auto result = callbacks_.on_ladder(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_char_ladder(
    const uint8_t* payload, size_t len)
{
    // Wire layout (after 3-byte header):
    //   [0..3]  uint32_t  hardcore   (hardcore flag, LE)
    //   [4..7]  uint32_t  expansion  (expansion flag, LE)
    //   [8..]   char[]    char_name  (null-terminated)
    constexpr size_t kMinFixed = 8;
    if (len < kMinFixed) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument,
                             "D2CS CHARLADDERREQ: payload too short"));
    }

    D2CSCharLadderRequest req;
    size_t offset = 0;
    if (!read_u32le(payload, len, offset, req.hardcore))  goto short_payload;
    if (!read_u32le(payload, len, offset, req.expansion)) goto short_payload;

    if (!read_cstring(payload, len, offset, req.char_name)) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "D2CS CHARLADDERREQ: unterminated char_name"));
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
                                       "D2CS CHARLADDERREQ: payload too short"));
}

}  // namespace pvpgn::protocol::d2cs
