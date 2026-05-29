// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_anongame_cancel_bridge.hpp"

#include <array>
#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"

// ---------------------------------------------------------------------------
// Wire layout for SERVER_FINDANONGAME_PLAYGAME_CANCEL (packet_class_bnet, SID 0x44)
//
//  Offset  Size  Field
//  ------  ----  -----
//   0      1     cancel  (uint8,  SERVER_FINDANONGAME_CANCEL = 0x03)
//   1      4     count   (uint32, little-endian)
//
// Total body: 5 bytes.
// The bnet framing header (4 bytes: 0xFF, SID, size_lo, size_hi) is prepended
// by the legacy conn_push_outqueue / packet_class_bnet machinery, so the
// bridge only needs to supply the body bytes.
// ---------------------------------------------------------------------------

namespace {

// SERVER_FINDANONGAME_CANCEL option byte value (matches legacy constant).
constexpr std::uint8_t kOptionCancel = 0x03u;

// Packet body size: 1 (cancel) + 4 (count).
constexpr std::size_t kBodySize = 5u;

// Write a 32-bit little-endian value into buf at offset.
inline void write_le32(unsigned char* buf, std::size_t off, std::uint32_t v) noexcept {
    buf[off + 0] = static_cast<unsigned char>(v & 0xFFu);
    buf[off + 1] = static_cast<unsigned char>((v >> 8)  & 0xFFu);
    buf[off + 2] = static_cast<unsigned char>((v >> 16) & 0xFFu);
    buf[off + 3] = static_cast<unsigned char>((v >> 24) & 0xFFu);
}

}  // namespace

extern "C" int pvpgn_v3_send_anongame_cancel(void*        conn_ptr,
                                              unsigned int count) noexcept {
    if (conn_ptr == nullptr) return 0;

    std::array<unsigned char, kBodySize> body{};
    body[0] = kOptionCancel;
    write_le32(body.data(), 1, static_cast<std::uint32_t>(count));

    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        body.data(),
        static_cast<unsigned int>(kBodySize));
}
