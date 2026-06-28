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
///   make_char_list_reply()     — CHARLISTREPLY (0x17)
///   make_create_char_reply()   — CREATECHARREPLY (0x03)
///   make_delete_char_reply()   — DELETECHARREPLY (0x04)
///   make_motd_reply()          — MOTDREPLY (0x13)
///   make_create_game_wait()    — CREATEGAMEWAIT (0x0F)
///   make_convert_char_reply()  — CONVERTCHARREPLY (0x1A)

#include "protocol/d2cs/fsm.hpp"

#include <cstddef>
#include <cstdint>
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
    uint16_t maxchar_field,
    const std::vector<charlistreply::CharEntry>& entries)
{
    // CHARLISTREPLY (0x17) body per d2cs_protocol.h t_d2cs_client_charlistreply:
    //   maxchar(u16) currchar(u16) u1(u16=0) currchar2(u16)
    //   then, per char: NUL-terminated name + NUL-terminated portrait block.
    // The byte-accurate `charlistreply::encode()` produces this exact layout
    // (including the 3-byte framed header), so just adapt its std::byte output
    // to the std::vector<uint8_t> wire buffer used by the egress.
    const auto bytes = charlistreply::encode(maxchar_field, entries);

    std::vector<uint8_t> v;
    v.reserve(bytes.size());
    for (std::byte b : bytes) {
        v.push_back(static_cast<uint8_t>(b));
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
    // Per d2cs_protocol.h t_d2cs_client_deletecharreply: Header(3) + u1(u16,
    // "always zero") + reply(u32) = 9 bytes. The previous 7-byte encoding
    // omitted the u1 short, misframing the reply for a real D2 client.
    std::vector<uint8_t> v;
    v.reserve(9);
    push_header(v, 9, D2CSPacketType::DELETECHARREPLY);
    v.push_back(0x00);  // u1 (always zero)
    v.push_back(0x00);
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_motd_reply(std::string_view message) {
    // Per d2cs_protocol.h t_d2cs_client_motdreply: Header(3) + u1(1) + message
    // (null-terminated). The original leaves the u1 byte zero.
    const uint16_t total = static_cast<uint16_t>(3 + 1 + message.size() + 1);
    std::vector<uint8_t> v;
    v.reserve(total);
    push_header(v, total, D2CSPacketType::MOTDREPLY);
    v.push_back(0x00);  // u1
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
