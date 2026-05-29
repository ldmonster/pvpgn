// SPDX-License-Identifier: GPL-2.0-or-later
//
// Minimal Battle.net wire-protocol primitives for the v3 client
// tools (bnchat / bnftp / bnbot / bnstat).  Header-only.
//
// This is deliberately *not* a port of the full server-side codec.
// It covers only the pieces the four client binaries need:
//   * little-endian byte-array PODs that mirror `common/bn_type.h`
//     (`bn_byte`, `bn_short`, `bn_int`) plus accessor helpers;
//   * the `packet_class` discriminator;
//   * the 4-byte BNet header (`type`, `size` -- both little-endian
//     `uint16_t`);
//   * the single-byte init-conn class octet;
//   * a `Packet` buffer with framing helpers (`recv_bnet`,
//     `send_bnet`, `recv_init_classbyte`, `send_init_classbyte`).
//
// Per-tool packet bodies (auth-info, ping, getfile-req, ...) are
// kept inside each tool's `.cpp` so this header stays slim.

#ifndef PVPGN_V3_CLIENT_BNCLIENT_PROTO_HPP
#define PVPGN_V3_CLIENT_BNCLIENT_PROTO_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "bnclient_net.hpp"

namespace pvpgn::client_v3::proto {

// ---- little-endian byte-array PODs (mirror common/bn_type.h) -----------

using bn_byte  = std::array<std::uint8_t, 1>;
using bn_short = std::array<std::uint8_t, 2>;
using bn_int   = std::array<std::uint8_t, 4>;
using bn_long  = std::array<std::uint8_t, 8>;

static_assert(sizeof(bn_byte)  == 1);
static_assert(sizeof(bn_short) == 2);
static_assert(sizeof(bn_int)   == 4);
static_assert(sizeof(bn_long)  == 8);

// Host-byte-order accessors.

inline std::uint8_t  byte_get (const bn_byte&  s) noexcept { return s[0]; }
inline std::uint16_t short_get(const bn_short& s) noexcept {
    return static_cast<std::uint16_t>(s[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(s[1]) << 8);
}
inline std::uint32_t int_get  (const bn_int&   s) noexcept {
    return static_cast<std::uint32_t>(s[0])
         | (static_cast<std::uint32_t>(s[1]) <<  8)
         | (static_cast<std::uint32_t>(s[2]) << 16)
         | (static_cast<std::uint32_t>(s[3]) << 24);
}

inline void byte_set (bn_byte&  d, std::uint8_t v)  noexcept { d[0] = v; }
inline void short_set(bn_short& d, std::uint16_t v) noexcept {
    d[0] = static_cast<std::uint8_t>(v & 0xff);
    d[1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
}
inline void int_set  (bn_int&   d, std::uint32_t v) noexcept {
    d[0] = static_cast<std::uint8_t>(v & 0xff);
    d[1] = static_cast<std::uint8_t>((v >>  8) & 0xff);
    d[2] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    d[3] = static_cast<std::uint8_t>((v >> 24) & 0xff);
}

// "n" variants are the network-byte-order (big-endian) twins used by
// a couple of legacy packets (e.g. embedded IPv4 addresses).
inline std::uint16_t short_nget(const bn_short& s) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(s[0]) << 8) |
           static_cast<std::uint16_t>(s[1]);
}
inline std::uint32_t int_nget  (const bn_int& s) noexcept {
    return (static_cast<std::uint32_t>(s[0]) << 24)
         | (static_cast<std::uint32_t>(s[1]) << 16)
         | (static_cast<std::uint32_t>(s[2]) <<  8)
         |  static_cast<std::uint32_t>(s[3]);
}
inline void int_nset(bn_int& d, std::uint32_t v) noexcept {
    d[0] = static_cast<std::uint8_t>((v >> 24) & 0xff);
    d[1] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    d[2] = static_cast<std::uint8_t>((v >>  8) & 0xff);
    d[3] = static_cast<std::uint8_t>( v        & 0xff);
}

// 4cc tag <-> bn_int.  Tag is stored little-endian on the wire so
// "IX86" appears as the bytes '6','8','X','I'.
inline void int_tag_set(bn_int& d, const char tag[4]) noexcept {
    d[0] = static_cast<std::uint8_t>(tag[3]);
    d[1] = static_cast<std::uint8_t>(tag[2]);
    d[2] = static_cast<std::uint8_t>(tag[1]);
    d[3] = static_cast<std::uint8_t>(tag[0]);
}
inline void int_tag_get(const bn_int& s, char out[5]) noexcept {
    out[0] = static_cast<char>(s[3]);
    out[1] = static_cast<char>(s[2]);
    out[2] = static_cast<char>(s[1]);
    out[3] = static_cast<char>(s[0]);
    out[4] = '\0';
}

// ---- packet class & header ---------------------------------------------

enum class PacketClass : std::uint8_t {
    None,
    Init,    // single-byte class octet, no header
    Bnet,    // 4-byte LE header { type, size } -- size includes header
    File,    // 4-byte header (size, version-ish)
    Raw,     // no header
    Udp,     // 4-byte LE header { type, size }
};

// Init-conn class octets (cf. common/init_protocol.h).
namespace init_class {
    inline constexpr std::uint8_t Bnet   = 0x01;
    inline constexpr std::uint8_t File   = 0x02;
    inline constexpr std::uint8_t Bot    = 0x03;
    inline constexpr std::uint8_t Telnet = 0x0d;
}

// BNet header: two little-endian uint16s.  `size` is the total
// packet length on the wire (header + body).
struct BnetHeader {
    bn_short type;
    bn_short size;
};
static_assert(sizeof(BnetHeader) == 4);

inline constexpr std::size_t kBnetHeaderSize = 4;
inline constexpr std::size_t kMaxPacketSize  = 0xffff;

// ---- Packet buffer ------------------------------------------------------

// A `Packet` is a fixed-capacity scratch buffer used to assemble or
// receive one BNet message.  It does not own a socket; the caller
// drives I/O via the free functions below.
class Packet {
public:
    static constexpr std::size_t capacity = kMaxPacketSize;

    Packet() noexcept = default;

    void clear() noexcept {
        size_   = 0;
        std::memset(buf_.data(), 0, buf_.size());
    }

    std::uint8_t*       data()       noexcept { return buf_.data(); }
    const std::uint8_t* data() const noexcept { return buf_.data(); }

    std::size_t size() const noexcept { return size_; }
    void        resize(std::size_t n) noexcept { size_ = n; }

    // BNet-class helpers.  These poke / read the 4-byte LE header at
    // offset 0.  Callers must have already set the size with
    // `set_bnet_size()` before sending.
    std::uint16_t bnet_type() const noexcept {
        BnetHeader h{};
        std::memcpy(&h, buf_.data(), sizeof(h));
        return short_get(h.type);
    }
    std::uint16_t bnet_size() const noexcept {
        BnetHeader h{};
        std::memcpy(&h, buf_.data(), sizeof(h));
        return short_get(h.size);
    }
    void set_bnet_type(std::uint16_t t) noexcept {
        BnetHeader h{};
        std::memcpy(&h, buf_.data(), sizeof(h));
        short_set(h.type, t);
        std::memcpy(buf_.data(), &h, sizeof(h));
    }
    void set_bnet_size(std::uint16_t s) noexcept {
        BnetHeader h{};
        std::memcpy(&h, buf_.data(), sizeof(h));
        short_set(h.size, s);
        std::memcpy(buf_.data(), &h, sizeof(h));
        size_ = s;
    }

    // Treat the body (after the 4-byte header) as a POD `T`.
    template <class T>
    T* body_as() noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        return reinterpret_cast<T*>(buf_.data() + kBnetHeaderSize);
    }
    template <class T>
    const T* body_as() const noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        return reinterpret_cast<const T*>(buf_.data() + kBnetHeaderSize);
    }

    // Append raw bytes at the current size.  Returns false if it
    // would overflow the buffer.
    bool append(const void* src, std::size_t n) noexcept {
        if (size_ + n > buf_.size()) {
            return false;
        }
        std::memcpy(buf_.data() + size_, src, n);
        size_ += n;
        return true;
    }
    bool append_cstr(const char* s) noexcept {
        const std::size_t n = std::strlen(s) + 1;  // include NUL
        return append(s, n);
    }

private:
    std::array<std::uint8_t, capacity> buf_{};
    std::size_t                        size_{0};
};

// ---- framing on a socket ----------------------------------------------

// Send the single-octet "connection class" used to multiplex the
// BNet / BNFTP / BNBOT entry points on TCP port 6112.
inline bool send_init_classbyte(net::socket_t sd, std::uint8_t cls) noexcept {
    return net::send_all(sd, &cls, 1);
}

// Send a fully-formed BNet packet.  The buffer's `bnet_size()` must
// already reflect the total wire length.  Returns false on I/O
// failure or framing inconsistency.
inline bool send_bnet(net::socket_t sd, const Packet& p) noexcept {
    if (p.size() < kBnetHeaderSize) {
        return false;
    }
    if (p.bnet_size() != p.size()) {
        return false;
    }
    return net::send_all(sd, p.data(), p.size());
}

// Receive one BNet packet.  Reads the 4-byte header, validates the
// size, then reads the body.  On success the buffer's size matches
// the wire size.  Returns false on EOF, short read, or oversized
// header.
inline bool recv_bnet(net::socket_t sd, Packet& p) noexcept {
    p.clear();
    if (!net::recv_all(sd, p.data(), kBnetHeaderSize)) {
        return false;
    }
    const auto wire = p.bnet_size();
    if (wire < kBnetHeaderSize || wire > Packet::capacity) {
        return false;
    }
    p.resize(wire);
    if (wire == kBnetHeaderSize) {
        return true;  // header-only packet
    }
    return net::recv_all(sd, p.data() + kBnetHeaderSize, wire - kBnetHeaderSize);
}

// Read exactly `len` raw bytes into `p` (no framing).  Used by the
// file-transfer / bot classes that do not use the BNet header.
inline bool recv_raw(net::socket_t sd, Packet& p, std::size_t len) noexcept {
    if (len > Packet::capacity) {
        return false;
    }
    p.clear();
    p.resize(len);
    if (len == 0) {
        return true;
    }
    return net::recv_all(sd, p.data(), len);
}

} // namespace pvpgn::client_v3::proto

#endif // PVPGN_V3_CLIENT_BNCLIENT_PROTO_HPP
