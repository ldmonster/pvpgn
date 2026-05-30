// SPDX-License-Identifier: GPL-2.0-or-later
// main/bnet_framer.hpp — TCP stream framing buffer for the BNet protocol.
//
// BNet uses a 4-byte header (0xFF, SID, length-LE-uint16) followed by a
// variable-length payload. TCP delivers a byte stream, so we accumulate
// bytes here until a complete packet is available, then decode and forward.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/bytes.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/common/packet.hpp"

namespace pvpgn::app::bnetd {

struct BnetFramer {
    std::vector<std::byte> buf;

    /// Feed raw bytes; call `fn` for each complete decoded ClientMessage.
    template <class Fn>
    void feed(core::ByteView incoming, Fn&& fn) {
        buf.insert(buf.end(), incoming.begin(), incoming.end());

        while (buf.size() >= protocol::BnetHeader::kSize) {
            auto hdr_result = protocol::parse_bnet_header(
                core::ByteView{buf.data(), buf.size()});
            if (!hdr_result) break;  // malformed — caller should close

            const std::uint16_t pkt_size = hdr_result.value().size;
            if (buf.size() < pkt_size) break;  // incomplete packet

            // We have a complete packet — parse_packet fills header + payload.
            auto fp_result = protocol::parse_packet(
                core::ByteView{buf.data(), pkt_size});
            if (!fp_result) {
                buf.erase(buf.begin(), buf.begin() + pkt_size);
                continue;
            }
            auto msg_result = protocol::bnet::decode_client(fp_result.value().packet);
            if (msg_result) {
                fn(std::move(msg_result.value()));
            }
            // Consume the packet bytes regardless of decode success.
            buf.erase(buf.begin(),
                      buf.begin() + static_cast<std::ptrdiff_t>(pkt_size));
        }
    }
};

} // namespace pvpgn::app::bnetd
