// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_anongame_search_reply_bridge.hpp"

#include <array>
#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"

// ---------------------------------------------------------------------------
// Wire layout for SERVER_ANONGAME_SEARCH_REPLY (packet_class_bnet, SID 0x46)
//
//  Offset  Size  Field
//  ------  ----  -----
//   0      1     option      (uint8,  SERVER_FINDANONGAME_SEARCH = 0x01)
//   1      4     count       (uint32, little-endian)
//   5      4     reply       (uint32, little-endian; 0 = queued)
//   9      2     search_time (uint16, little-endian; average seconds)
//
// Total body: 11 bytes.
// The bnet framing header (4 bytes: 0xFF, SID, size_lo, size_hi) is prepended
// by the legacy conn_push_outqueue / packet_class_bnet machinery, so the
// bridge only needs to supply the body bytes.
// ---------------------------------------------------------------------------

namespace {

// SERVER_FINDANONGAME_SEARCH option byte value (matches legacy constant).
constexpr std::uint8_t kOptionSearch = 0x01u;

// Packet body size: 1 (option) + 4 (count) + 4 (reply) + 2 (search_time).
constexpr std::size_t kBodySize = 11u;

// Write a 32-bit little-endian value into buf at offset.
inline void write_le32(unsigned char* buf, std::size_t off, std::uint32_t v) noexcept {
    buf[off + 0] = static_cast<unsigned char>(v & 0xFFu);
    buf[off + 1] = static_cast<unsigned char>((v >> 8)  & 0xFFu);
    buf[off + 2] = static_cast<unsigned char>((v >> 16) & 0xFFu);
    buf[off + 3] = static_cast<unsigned char>((v >> 24) & 0xFFu);
}

// Write a 16-bit little-endian value into buf at offset.
inline void write_le16(unsigned char* buf, std::size_t off, std::uint16_t v) noexcept {
    buf[off + 0] = static_cast<unsigned char>(v & 0xFFu);
    buf[off + 1] = static_cast<unsigned char>((v >> 8) & 0xFFu);
}

}  // namespace

extern "C" int pvpgn_v3_send_anongame_search_reply(void*          conn_ptr,
                                                    unsigned int   count,
                                                    unsigned int   reply,
                                                    unsigned short search_time) noexcept {
    if (conn_ptr == nullptr) return 0;

    std::array<unsigned char, kBodySize> body{};
    body[0] = kOptionSearch;
    write_le32(body.data(), 1, static_cast<std::uint32_t>(count));
    write_le32(body.data(), 5, static_cast<std::uint32_t>(reply));
    write_le16(body.data(), 9, static_cast<std::uint16_t>(search_time));

    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        body.data(),
        static_cast<unsigned int>(kBodySize));
}
