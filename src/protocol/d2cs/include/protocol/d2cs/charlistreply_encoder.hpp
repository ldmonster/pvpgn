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
inline constexpr std::uint8_t  kPacketType110 = 0x19;  // 1.10+ CHARLISTREPLY
inline constexpr std::size_t   kHeaderSize    = 3;
inline constexpr std::size_t   kReplyBaseSize = 11;  // header + 4 LE u16

struct CharEntry {
    std::string            charname;       // NUL-terminator added by encoder
    std::vector<std::byte> portrait;       // NUL-terminator added by encoder
    std::uint32_t          expire_time = 0; // emitted ONLY by encode_110 (0x19)
};

namespace detail {

inline void put_u8(std::vector<std::byte>& out, std::uint8_t v) {
    out.push_back(static_cast<std::byte>(v));
}
inline void put_u16le(std::vector<std::byte>& out, std::uint16_t v) {
    out.push_back(static_cast<std::byte>(v & 0xFF));
    out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
}
inline void put_u32le(std::vector<std::byte>& out, std::uint32_t v) {
    out.push_back(static_cast<std::byte>(v & 0xFF));
    out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::byte>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::byte>((v >> 24) & 0xFF));
}

}  // namespace detail

/// Shared encoder for both CHARLISTREPLY variants. The header layout is
/// identical (3-byte header + maxchar/currchar/u1/currchar2, all LE u16); the
/// 1.10 variant (0x19) differs only by the type byte and a leading 4-byte
/// expire_time before each character's name+portrait. See
/// `on_client_charlistreq` (0x17) and `on_client_charlistreq_110` (0x19) in the
/// legacy handle_d2cs.cpp.
inline std::vector<std::byte> encode_typed(std::uint8_t  packet_type,
                                           bool          with_expire_time,
                                           std::uint16_t maxchar_field,
                                           const std::vector<CharEntry>& entries) {
    std::vector<std::byte> body;
    for (const auto& e : entries) {
        if (with_expire_time) detail::put_u32le(body, e.expire_time);
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
    detail::put_u8(out, packet_type);
    detail::put_u16le(out, maxchar_field);
    detail::put_u16le(out, currchar);
    detail::put_u16le(out, 0);         // u1
    detail::put_u16le(out, currchar);  // currchar2
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

/// 1.10+ CHARLISTREPLY (0x19): each entry is prefixed with a 4-byte LE
/// expire_time, otherwise identical to the 0x17 reply.
inline std::vector<std::byte> encode_110(std::uint16_t maxchar_field,
                                         const std::vector<CharEntry>& entries) {
    return encode_typed(kPacketType110, /*with_expire_time=*/true,
                        maxchar_field, entries);
}

/// Encode a single CHARLISTREPLY packet.
///
/// \param maxchar_field  Value to write into the `maxchar` slot.
/// \param entries        Entries in the order they should appear on the
///                       wire (caller pre-sorts ASC or DESC).
inline std::vector<std::byte> encode(std::uint16_t                maxchar_field,
                                     const std::vector<CharEntry>& entries) {
    return encode_typed(kPacketType, /*with_expire_time=*/false,
                        maxchar_field, entries);
}

}  // namespace pvpgn::protocol::d2cs::charlistreply
