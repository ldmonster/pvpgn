// SPDX-License-Identifier: GPL-2.0-or-later
/// @file d2cs_builders.cpp
/// D2CSSessionFsm — outbound packet builder methods.
///
/// Each make_*() method constructs a fully-framed D2CS reply packet
/// (3-byte header + payload) ready to be written to the wire.
///
///   make_login_reply()         — LOGINREPLY (0x01)
///   make_char_login_reply()    — CHARLOGINREPLY (0x07)
///   make_create_game_reply()   — CREATEGAMEREPLY (0x03)
///   make_join_game_reply()     — JOINGAMEREPLY (0x04)
///   make_char_list_reply()     — CHARLISTREPLY (0x17)
///   make_create_char_reply()   — CREATECHARREPLY (0x02)
///   make_delete_char_reply()   — DELETECHARREPLY (0x0A)
///   make_motd_reply()          — MOTDREPLY (0x12)
///   make_create_game_wait()    — CREATEGAMEWAIT (0x14)
///   make_convert_char_reply()  — CONVERTCHARREPLY (0x18)

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
    uint32_t seqno, uint32_t game_id, uint32_t result_code, uint16_t u1)
{
    // Per t_d2cs_client_creategamereply (and wire_types CreateGameReply):
    // Header(3) + seqno(u16) + gameid(u16) + u1(u16) + reply(u32) = 13 bytes.
    // seqno/gameid/u1 are bn_short (u16) on the wire, NOT u32.
    std::vector<uint8_t> v;
    v.reserve(13);
    push_header(v, 13, D2CSPacketType::CREATEGAMEREPLY);
    push_u16le(v, static_cast<uint16_t>(seqno));
    push_u16le(v, static_cast<uint16_t>(game_id));
    push_u16le(v, u1);
    push_u32le(v, result_code);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_join_game_reply(
    uint32_t seqno, uint32_t game_id, uint32_t gs_ip,
    uint32_t token, uint32_t result_code)
{
    // Per t_d2cs_client_joingamereply (and wire_types JoinGameReply):
    // Header(3) + seqno(u16) + gameid(u16) + u1(u16=0) + addr(u32) + token(u32)
    // + reply(u32) = 21 bytes. seqno/gameid/u1 are bn_short (u16), NOT u32.
    std::vector<uint8_t> v;
    v.reserve(21);
    push_header(v, 21, D2CSPacketType::JOINGAMEREPLY);
    push_u16le(v, static_cast<uint16_t>(seqno));
    push_u16le(v, static_cast<uint16_t>(game_id));
    push_u16le(v, 0);           // u1 (reserved)
    push_u32be(v, gs_ip);       // addr: network/big-endian (oracle bn_int_nset)
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

std::vector<uint8_t> D2CSSessionFsm::make_char_list_reply_110(
    uint16_t maxchar_field,
    const std::vector<charlistreply::CharEntry>& entries)
{
    // CHARLISTREPLY_110 (0x19): same header as 0x17 but each per-char entry is
    // prefixed with a 4-byte LE expire_time (see charlistreply::encode_110 and
    // the legacy on_client_charlistreq_110 in handle_d2cs.cpp).
    const auto bytes = charlistreply::encode_110(maxchar_field, entries);

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

std::vector<uint8_t> D2CSSessionFsm::make_game_list_entry(
    uint16_t seqno, uint32_t token, uint8_t currchar, uint32_t gameflag,
    std::string_view name, std::string_view desc)
{
    // Per t_d2cs_client_gamelistreply: Header(3) + seqno(u16) + token(u32) +
    // currchar(u8) + gameflag(u32) + name\0 + desc\0.
    const uint16_t total = static_cast<uint16_t>(
        3 + 2 + 4 + 1 + 4 + name.size() + 1 + desc.size() + 1);
    std::vector<uint8_t> v;
    v.reserve(total);
    push_header(v, total, D2CSPacketType::GAMELISTREPLY);
    push_u16le(v, seqno);
    push_u32le(v, token);
    push_u8(v, currchar);
    push_u32le(v, gameflag);
    v.insert(v.end(), name.begin(), name.end());
    v.push_back(0x00);
    v.insert(v.end(), desc.begin(), desc.end());
    v.push_back(0x00);
    return v;
}

std::vector<uint8_t> D2CSSessionFsm::make_game_list_terminator(uint16_t seqno) {
    // End-of-list marker: token/currchar/gameflag zero + THREE empty strings
    // (the original's trailing packet appends "" three times).
    std::vector<uint8_t> v;
    v.reserve(17);
    push_header(v, 17, D2CSPacketType::GAMELISTREPLY);
    push_u16le(v, seqno);
    push_u32le(v, 0);  // token
    push_u8(v, 0);     // currchar
    push_u32le(v, 0);  // gameflag
    push_u8(v, 0);     // ""
    push_u8(v, 0);     // ""
    push_u8(v, 0);     // ""
    return v;
}

}  // namespace pvpgn::protocol::d2cs
