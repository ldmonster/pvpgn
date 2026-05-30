// SPDX-License-Identifier: GPL-2.0-or-later
/// @file d2cs_handlers.cpp
/// D2CSSessionFsm — packet handler methods.
///
/// Each handler parses the raw payload bytes into a typed request struct
/// and fires the corresponding application-layer callback.  State
/// transitions (authenticating → authenticated → in_game) are applied here.
///
///   handle_login()              — LOGINREQ (0x01)
///   handle_char_login()         — CHARLOGINREQ (0x0A)
///   handle_create_game()        — CREATEGAMEREQ (0x07)
///   handle_join_game()          — JOINGAMEREQ (0x08)
///   handle_game_list()          — GAMELISTREQ (0x09)
///   handle_game_info()          — GAMEINFOREQ (0x0B)
///   handle_create_char()        — CREATECHARREQ (0x02)
///   handle_delete_char()        — DELETECHARREQ (0x03)
///   handle_char_list()          — CHARLISTREQ (0x17)
///   handle_char_list_110()      — CHARLISTREQ110 (0x18)
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
