// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm.cpp
/// D2CSSessionFsm — thin coordinator.
///
/// This translation unit owns only the framing layer:
///   - Construction / destruction
///   - feed()     — buffer management + packet framing
///   - dispatch() — opcode → handler routing
///
/// All handler, builder, and I/O-helper implementations live in the
/// focused sub-TUs under fsm/:
///   fsm/d2cs_io.cpp       — read_cstring / read_u*le / push_* helpers
///   fsm/d2cs_handlers.cpp — handle_* methods (one per D2CS opcode)
///   fsm/d2cs_builders.cpp — make_* packet-builder methods

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
// Feed — buffer management + packet framing
// ===========================================================================

core::Result<size_t, core::Error> D2CSSessionFsm::feed(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return core::Result<size_t, core::Error>(static_cast<size_t>(0));
    }

    buffer_.insert(buffer_.end(), data, data + len);
    size_t consumed = 0;

    while (buffer_.size() >= kHeaderSize) {
        // Parse header: length (2 bytes LE) + type (1 byte)
        const uint16_t packet_len = static_cast<uint16_t>(
            static_cast<uint16_t>(buffer_[0]) |
            (static_cast<uint16_t>(buffer_[1]) << 8));

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
// Dispatch — opcode → handler routing
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
            return handle_char_list(payload, len);
        case D2CSPacketType::CHARLISTREQ110:
            return handle_char_list_110(payload, len);
        case D2CSPacketType::MOTDREQ:
            return handle_motd(payload, len);
        case D2CSPacketType::CANCELCREATEGAME:
            return handle_cancel_create_game(payload, len);
        case D2CSPacketType::CONVERTCHARREQ:
            return handle_convert_char(payload, len);
        case D2CSPacketType::LADDERREQ:
            return handle_ladder(payload, len);
        case D2CSPacketType::CHARLADDERREQ:
            return handle_char_ladder(payload, len);
        default:
            // Unknown packet type — log and ignore rather than killing the session.
            // This matches legacy behaviour in handle_d2cs.cpp where unrecognised
            // opcodes are silently dropped.
            return core::Result<void, core::Error>();
    }
}

}  // namespace pvpgn::protocol::d2cs
