// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladderreply_encoder.hpp
/// Byte-accurate v3 encoder for the legacy D2CS_CLIENT_LADDERREPLY
/// (packet type 0x11) wire format produced by
/// `d2cs_send_client_ladder` in `src/d2cs/handle_d2cs.cpp`.
///
/// One call returns the *full pagination* -- one or more packets,
/// each already prefixed with the standard 3-byte d2cs client header
/// (size LE u16, type u8 = 0x11). The legacy 4-byte truncation quirk
/// applied on the first packet (`packet_set_size(rpacket,
/// packet_get_size(rpacket) - 4); curr_len -= 4;`) is reproduced
/// faithfully.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::protocol::d2cs::ladderreply {

inline constexpr std::uint8_t  kPacketType            = 0x11;
inline constexpr std::size_t   kHeaderSize            = 3;   // size(2) + type(1)
inline constexpr std::size_t   kReplyBaseSize         = 10;  // header + type/total_len/curr_len/cont_len
inline constexpr std::size_t   kLadderHeaderSize      = 8;   // start_pos(2) + u1(2) + count1(4)
inline constexpr std::size_t   kLadderInfoHeaderSize  = 4;   // count2(4)
inline constexpr std::size_t   kLadderInfoSize        = 28;  // explow(4) + exphigh(4) + status(2) + level(1) + u1(1) + charname[16]
inline constexpr std::size_t   kEntriesPerPacket      = 14;

/// One ladder entry, mirrors `t_d2cs_client_ladderinfo` byte layout.
struct LadderInfo {
    std::uint32_t exp_low;
    std::uint32_t exp_high;   // legacy code documents this as "always zero"
    std::uint16_t status;
    std::uint8_t  level;
    std::uint8_t  u1;         // "always zero"
    std::array<char, 16> charname;  // NUL-padded, not necessarily NUL-terminated
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
inline void put_ladder_info(std::vector<std::byte>& out, const LadderInfo& li) {
    put_u32le(out, li.exp_low);
    put_u32le(out, li.exp_high);
    put_u16le(out, li.status);
    put_u8(out, li.level);
    put_u8(out, li.u1);
    for (char c : li.charname) {
        out.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(c)));
    }
}

}  // namespace detail

/// One emitted packet. `bytes[0..2]` already hold the d2cs client header
/// (size LE u16 = bytes.size(), type u8 = 0x11).
struct EmittedPacket {
    std::vector<std::byte> bytes;
};

/// Encode the full `D2CS_CLIENT_LADDERREPLY` paginated stream.
///
/// @param type        Ladder type byte (D2LADDER_*).
/// @param start_pos   Starting position as returned by `d2ladder_get_ladder`.
/// @param entries     The ladder entries to emit (`count` in legacy code).
///                    Pagination is `kEntriesPerPacket = 14` per packet.
///
/// When `entries.empty()` the encoder emits the single "empty reply"
/// shape used by `on_client_charladderreq` on lookup miss:
/// 10-byte packet with all length fields = 0.
inline std::vector<EmittedPacket>
encode(std::uint8_t              type,
       std::uint16_t             start_pos,
       const std::vector<LadderInfo>& entries) {
    std::vector<EmittedPacket> packets;

    if (entries.empty()) {
        EmittedPacket p;
        detail::put_u16le(p.bytes, static_cast<std::uint16_t>(kReplyBaseSize));
        detail::put_u8(p.bytes, kPacketType);
        detail::put_u8(p.bytes, type);
        detail::put_u16le(p.bytes, 0);  // total_len
        detail::put_u16le(p.bytes, 0);  // curr_len
        detail::put_u16le(p.bytes, 0);  // cont_len
        packets.push_back(std::move(p));
        return packets;
    }

    const auto count = static_cast<std::uint32_t>(entries.size());
    const std::uint32_t npacket =
        (count + static_cast<std::uint32_t>(kEntriesPerPacket) - 1u)
        / static_cast<std::uint32_t>(kEntriesPerPacket);

    // total_len = count * 28 + 8 (ladderheader) + 4 * npacket (infoheaders)
    //             - 4 (legacy correction)
    const std::uint32_t total_len_full =
        count * static_cast<std::uint32_t>(kLadderInfoSize)
              + static_cast<std::uint32_t>(kLadderHeaderSize)
              + static_cast<std::uint32_t>(kLadderInfoHeaderSize) * npacket;
    const std::uint16_t total_len =
        static_cast<std::uint16_t>(total_len_full - 4);

    std::uint16_t cont_len = 0;

    for (std::uint32_t i = 0; i < npacket; ++i) {
        std::vector<std::byte> body;  // bytes AFTER the 10-byte base
        std::uint32_t          curr_len = 0;

        if (i == 0) {
            // ladderheader (8 bytes): start_pos, u1=0, count1=count
            detail::put_u16le(body, start_pos);
            detail::put_u16le(body, 0);
            detail::put_u32le(body, count);
            curr_len += kLadderHeaderSize;
        }

        // infoheader: count2 = count on first packet, 0 elsewhere
        detail::put_u32le(body, (i == 0) ? count : 0u);
        curr_len += kLadderInfoHeaderSize;

        // Up to 14 ladder entries
        const std::uint32_t first_idx =
            i * static_cast<std::uint32_t>(kEntriesPerPacket);
        const std::uint32_t last_idx  = std::min(
            first_idx + static_cast<std::uint32_t>(kEntriesPerPacket), count);
        for (std::uint32_t n = first_idx; n < last_idx; ++n) {
            detail::put_ladder_info(body, entries[n]);
            curr_len += kLadderInfoSize;
        }

        // First-packet truncation quirk: drop trailing 4 bytes and
        // adjust curr_len accordingly. Legacy code does:
        //   packet_set_size(rpacket, packet_get_size(rpacket) - 4);
        //   curr_len -= 4;
        if (i == 0) {
            body.resize(body.size() - 4);
            curr_len -= 4;
        }

        EmittedPacket p;
        const std::uint16_t total_size =
            static_cast<std::uint16_t>(kReplyBaseSize + body.size());
        detail::put_u16le(p.bytes, total_size);
        detail::put_u8(p.bytes, kPacketType);
        detail::put_u8(p.bytes, type);
        detail::put_u16le(p.bytes, total_len);
        detail::put_u16le(p.bytes, static_cast<std::uint16_t>(curr_len));
        detail::put_u16le(p.bytes, cont_len);
        p.bytes.insert(p.bytes.end(), body.begin(), body.end());
        packets.push_back(std::move(p));

        cont_len = static_cast<std::uint16_t>(cont_len + curr_len);
    }

    return packets;
}

}  // namespace pvpgn::protocol::d2cs::ladderreply
