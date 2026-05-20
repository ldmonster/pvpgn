// SPDX-License-Identifier: GPL-2.0-or-later
//
// v3 byte-accurate encoder for `D2CS_CLIENT_CHARLISTREPLY` (0x17).
//
// Layout (all multi-byte fields little-endian):
//
//   bn_short size       (LE u16, including the 3-byte header)
//   bn_byte  type       (0x17)
//   bn_short maxchar    (LE u16; legacy: actual maxchar if allow_newchar
//                        and there is room, otherwise 0)
//   bn_short currchar   (LE u16; number of entries)
//   bn_short u1         (LE u16; always zero)
//   bn_short currchar2  (LE u16; same as currchar)
//   For each entry:
//       NUL-terminated charname
//       portrait bytes followed by a NUL (legacy comment says "each
//       portrait block is 0x22 bytes static length", but the legacy
//       code uses `packet_append_string` which writes the bytes up to
//       and including the first NUL).
//
// The legacy emitter lives in `src/d2cs/handle_d2cs.cpp`
// (`on_client_charlistreq`). Sort order (ASC vs. DESC) and the
// `allow_newchar`/error-path branching are caller decisions; this
// encoder simply emits entries in the given order with the given
// `maxchar_field` value.

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace pvpgn::protocol::d2cs::charlistreply {

inline constexpr std::uint8_t  kPacketType    = 0x17;
inline constexpr std::size_t   kHeaderSize    = 3;
inline constexpr std::size_t   kReplyBaseSize = 11;  // header + 4 LE u16

struct CharEntry {
    std::string            charname;  // NUL-terminator added by encoder
    std::vector<std::byte> portrait;  // NUL-terminator added by encoder
};

namespace detail {

inline void put_u8(std::vector<std::byte>& out, std::uint8_t v) {
    out.push_back(static_cast<std::byte>(v));
}
inline void put_u16le(std::vector<std::byte>& out, std::uint16_t v) {
    out.push_back(static_cast<std::byte>(v & 0xFF));
    out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
}

}  // namespace detail

/// Encode a single CHARLISTREPLY packet.
///
/// \param maxchar_field  Value to write into the `maxchar` slot.
/// \param entries        Entries in the order they should appear on the
///                       wire (caller pre-sorts ASC or DESC).
inline std::vector<std::byte> encode(std::uint16_t                maxchar_field,
                                     const std::vector<CharEntry>& entries) {
    std::vector<std::byte> body;
    for (const auto& e : entries) {
        for (char c : e.charname) {
            body.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(c)));
        }
        body.push_back(std::byte{0});
        for (std::byte b : e.portrait) body.push_back(b);
        body.push_back(std::byte{0});
    }

    const std::uint16_t currchar =
        static_cast<std::uint16_t>(entries.size());
    const std::uint16_t total_size =
        static_cast<std::uint16_t>(kReplyBaseSize + body.size());

    std::vector<std::byte> out;
    detail::put_u16le(out, total_size);
    detail::put_u8(out, kPacketType);
    detail::put_u16le(out, maxchar_field);
    detail::put_u16le(out, currchar);
    detail::put_u16le(out, 0);  // u1
    detail::put_u16le(out, currchar);  // currchar2
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

}  // namespace pvpgn::protocol::d2cs::charlistreply
