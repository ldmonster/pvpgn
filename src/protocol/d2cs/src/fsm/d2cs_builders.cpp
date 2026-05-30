// SPDX-License-Identifier: GPL-2.0-or-later
/// @file d2cs_builders.cpp
/// D2CSSessionFsm — outbound packet builder methods.
///
/// Each make_*() method constructs a fully-framed D2CS reply packet
/// (3-byte header + payload) ready to be written to the wire.
///
///   make_login_reply()         — LOGINREPLY (0x02)
///   make_char_login_reply()    — CHARLOGINREPLY (0x0B)
///   make_create_game_reply()   — CREATEGAMEREPLY (0x0D)
///   make_join_game_reply()     — JOINGAMEREPLY (0x0E)
///   make_char_list_reply()     — CHARLISTREPLY (0x18)
///   make_create_char_reply()   — CREATECHARREPLY (0x03)
///   make_delete_char_reply()   — DELETECHARREPLY (0x04)
///   make_motd_reply()          — MOTDREPLY (0x13)
///   make_create_game_wait()    — CREATEGAMEWAIT (0x0F)
///   make_convert_char_reply()  — CONVERTCHARREPLY (0x1A)

#include "protocol/d2cs/fsm.hpp"

#include <string_view>
#include <vector>

namespace pvpgn::protocol::d2cs {

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

}  // namespace pvpgn::protocol::d2cs
