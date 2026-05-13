// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file packet.hpp
/// Battle.net binary packet header + immutable byte view.
///
/// Wire layout (LE, 4 bytes):
///   uint8  marker   (always 0xFF for BNet 0x-class packets)
///   uint8  code     (message id)
///   uint16 size     (total packet length, header included)
///
/// The header is fixed-size; legacy code calls it `t_bnetd_hdr`.
/// This file does not commit to a specific protocol: codecs in
/// `protocol/<name>/` either reuse `BnetHeader` or define their own.
///
/// Pure value types; no I/O, no allocations.

#include <cstddef>
#include <cstdint>
#include <span>

#include "core/bytes.hpp"
#include "core/endian.hpp"
#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::protocol {

inline constexpr std::uint8_t kBnetMarker = 0xFF;

struct BnetHeader {
    std::uint8_t  marker = kBnetMarker;
    std::uint8_t  code   = 0;
    std::uint16_t size   = 0;  ///< Total packet size, header included.

    static constexpr std::size_t kSize = 4;

    constexpr bool operator==(const BnetHeader&) const = default;
};

/// Parse a 4-byte little-endian header. Returns `OutOfRange` if the
/// buffer is shorter than 4 bytes, `InvalidArgument` if the marker is
/// not 0xFF, or if the encoded size is smaller than the header itself.
inline core::Result<BnetHeader> parse_bnet_header(core::ByteView buf) {
    if (buf.size() < BnetHeader::kSize) {
        return core::fail(core::make_error(core::StatusCode::OutOfRange,
                                            "bnet header: short buffer"));
    }
    BnetHeader h;
    h.marker = static_cast<std::uint8_t>(buf[0]);
    h.code   = static_cast<std::uint8_t>(buf[1]);
    auto sz  = core::read_le<std::uint16_t>(buf.subspan(2, 2));
    if (!sz) return core::fail(sz.error());
    h.size = sz.value();
    if (h.marker != kBnetMarker) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                            "bnet header: bad marker"));
    }
    if (h.size < BnetHeader::kSize) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                            "bnet header: size < 4"));
    }
    return h;
}

/// Serialize a header into a 4-byte span. Returns `OutOfRange` if too small.
inline core::Status<> write_bnet_header(core::ByteSpan dst,
                                         const BnetHeader& h) {
    if (dst.size() < BnetHeader::kSize) {
        return core::fail(core::make_error(core::StatusCode::OutOfRange,
                                            "bnet header: short buffer"));
    }
    dst[0] = static_cast<std::byte>(h.marker);
    dst[1] = static_cast<std::byte>(h.code);
    return core::write_le<std::uint16_t>(dst.subspan(2, 2), h.size);
}

/// Owning-but-cheap framed packet: header + payload view into a buffer the
/// caller owns. Lifetime is the buffer's; do not let it dangle.
struct Packet {
    BnetHeader      header;
    core::ByteView  payload;  ///< Excludes the 4-byte header.
};

/// Parse one complete packet from `buf`. Returns the packet **plus**
/// the number of consumed bytes (== `header.size`). Caller is expected
/// to advance their stream cursor by the consumed count.
///
/// `OutOfRange` is returned if `buf` does not yet contain a full
/// packet (the caller should wait for more bytes); other errors are
/// terminal for the session.
struct FramedPacket {
    Packet      packet;
    std::size_t consumed = 0;
};

inline core::Result<FramedPacket> parse_packet(core::ByteView buf) {
    auto hdr = parse_bnet_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::make_error(core::StatusCode::OutOfRange,
                                            "packet: incomplete"));
    }
    FramedPacket fp;
    fp.packet.header  = hdr.value();
    fp.packet.payload = buf.subspan(BnetHeader::kSize,
                                    static_cast<std::size_t>(hdr.value().size) -
                                        BnetHeader::kSize);
    fp.consumed       = hdr.value().size;
    return fp;
}

}  // namespace pvpgn::protocol
