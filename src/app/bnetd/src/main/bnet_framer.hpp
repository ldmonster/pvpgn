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
    /// Set when an unrecoverably-corrupt header (declared size < 4) is seen;
    /// the caller should close the session, mirroring the original server which
    /// destroys connections whose total packet size is below the header size.
    bool wants_close = false;

    /// Feed raw bytes; call `fn` for each complete decoded ClientMessage.
    template <class Fn>
    void feed(core::ByteView incoming, Fn&& fn) {
        buf.insert(buf.end(), incoming.begin(), incoming.end());

        while (buf.size() >= protocol::BnetHeader::kSize) {
            auto hdr_result = protocol::parse_bnet_header(
                core::ByteView{buf.data(), buf.size()});
            if (!hdr_result) {
                // The original server does not validate the 0xFF marker — the
                // header is {uint16 type; uint16 size}, so a non-0xFF leading
                // byte just yields an unmatched type. It frames purely by the
                // 16-bit size field: an undecodable/unknown header is consumed
                // by its declared size and skipped, resyncing the stream. Mirror
                // that here instead of wedging the connection forever.
                auto sz = core::read_le<std::uint16_t>(
                    core::ByteView{buf.data(), buf.size()}.subspan(2, 2));
                if (!sz) break;  // cannot happen (buf.size() >= 4) — be safe
                const std::uint16_t bad_size = sz.value();
                if (bad_size < protocol::BnetHeader::kSize) {
                    // Truly corrupt: the original destroys such connections.
                    wants_close = true;
                    break;
                }
                if (buf.size() < bad_size) break;  // incomplete — await more
                // Drop the unknown packet by its declared size and resync.
                buf.erase(buf.begin(),
                          buf.begin() + static_cast<std::ptrdiff_t>(bad_size));
                continue;
            }

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
