// SPDX-License-Identifier: GPL-2.0-or-later
//
// Battle.net "broken SHA-1" hash, vendored for the v3 client tools.
// Cleanroom port of `src/common/bnethash.cpp` (the Blizzard variant
// only -- that's the only one any of the four client tools actually
// invokes; see `client_connect.cpp` password handshake).
//
// The algorithm is a non-standard SHA-1 used by classic
// Battle.net during authentication.  Do NOT use it for anything
// else; in particular it is not collision-resistant.  Output is
// five 32-bit little-endian "host order" words.
//
// Header-only.  No dependency on `core/` so it is also linkable
// from the compile probe.

#ifndef PVPGN_V3_CLIENT_BNCLIENT_HASH_HPP
#define PVPGN_V3_CLIENT_BNCLIENT_HASH_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace pvpgn::client_v3::hash {

// Output of a single Battle.net hash: five 32-bit host-order words.
using HashDigest = std::array<std::uint32_t, 5>;

namespace detail {

constexpr inline std::uint32_t rotl32(std::uint32_t x, int n) noexcept {
    return (x << n) | (x >> (32 - n));
}

inline void hash_init(HashDigest& h) noexcept {
    h[0] = 0x67452301u;
    h[1] = 0xefcdab89u;
    h[2] = 0x98badcfeu;
    h[3] = 0x10325476u;
    h[4] = 0xc3d2e1f0u;
}

// Single 80-round compression step, Blizzard "broken" variant
// (`ROTL32(1, x ^ y ^ z ^ w)` schedule instead of standard
// `ROTL32(x ^ y ^ z ^ w, 1)`).
inline void compress_block(HashDigest& h, std::uint32_t tmp[80]) noexcept {
    for (unsigned i = 0; i < 64; ++i) {
        tmp[i + 16] = rotl32(1, tmp[i] ^ tmp[i + 8] ^ tmp[i + 2] ^ tmp[i + 13]);
    }

    std::uint32_t a = h[0];
    std::uint32_t b = h[1];
    std::uint32_t c = h[2];
    std::uint32_t d = h[3];
    std::uint32_t e = h[4];
    std::uint32_t g = 0;

    unsigned i = 0;
    for (; i < 20; ++i) {
        g = tmp[i] + rotl32(a, 5) + e + ((b & c) | (~b & d)) + 0x5a827999u;
        e = d; d = c; c = rotl32(b, 30); b = a; a = g;
    }
    for (; i < 40; ++i) {
        g = (d ^ c ^ b) + e + rotl32(g, 5) + tmp[i] + 0x6ed9eba1u;
        e = d; d = c; c = rotl32(b, 30); b = a; a = g;
    }
    for (; i < 60; ++i) {
        g = tmp[i] + rotl32(g, 5) + e + ((c & b) | (d & c) | (d & b)) - 0x70e44324u;
        e = d; d = c; c = rotl32(b, 30); b = a; a = g;
    }
    for (; i < 80; ++i) {
        g = (d ^ c ^ b) + e + rotl32(g, 5) + tmp[i] - 0x359d3e2au;
        e = d; d = c; c = rotl32(b, 30); b = a; a = g;
    }

    h[0] += g;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

// Pack the first `count` bytes of `src` (up to 64) into 16 LE
// 32-bit words.  Trailing words / unused byte slots are zeroed.
// This matches the `do_blizzard_hash` branch of legacy
// `hash_set_16`.
inline void pack_blizzard(std::uint32_t dst[16],
                          const std::uint8_t* src,
                          unsigned count) noexcept {
    for (unsigned i = 0; i < 16; ++i) {
        std::uint32_t w = 0;
        for (unsigned b = 0; b < 4; ++b) {
            const unsigned pos = i * 4 + b;
            if (pos < count) {
                w |= static_cast<std::uint32_t>(src[pos]) << (b * 8);
            }
        }
        dst[i] = w;
    }
}

} // namespace detail

// Compute the Battle.net Blizzard-variant hash of `data` (length
// `len` bytes).  Equivalent to legacy `bnet_hash`.  The digest is
// returned as five host-order `uint32_t`s.
inline HashDigest bnet_hash(const void* data, std::size_t len) noexcept {
    HashDigest out{};
    detail::hash_init(out);

    const auto* p = static_cast<const std::uint8_t*>(data);
    while (len > 0) {
        const unsigned inc = (len > 64) ? 64u : static_cast<unsigned>(len);
        std::uint32_t  tmp[80]{};
        detail::pack_blizzard(tmp, p, inc);
        detail::compress_block(out, tmp);
        p   += inc;
        len -= inc;
    }
    return out;
}

// Convenience: hash a contiguous trivially-copyable object.
template <class T>
inline HashDigest bnet_hash_object(const T& obj) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    return bnet_hash(&obj, sizeof(obj));
}

// Format a digest as the classic 40-char ASCII representation used
// throughout the bnetd codebase -- five lowercase hex words, each
// printed LSB-first (i.e. `sprintf("%08x", host_word_swapped)`).
inline std::string to_string(const HashDigest& h) {
    // Each word is printed as if it were the byte-reversed version
    // of the host word: low-order hex digits of the byte-swapped
    // value go first.
    static constexpr char hex[] = "0123456789abcdef";
    std::string s;
    s.resize(40);
    for (unsigned i = 0; i < 5; ++i) {
        const std::uint32_t w = h[i];
        const std::uint32_t swapped =
            ((w & 0x000000ffu) << 24) |
            ((w & 0x0000ff00u) <<  8) |
            ((w & 0x00ff0000u) >>  8) |
            ((w & 0xff000000u) >> 24);
        for (unsigned b = 0; b < 8; ++b) {
            const auto shift = static_cast<int>((7 - b) * 4);
            s[i * 8 + b] = hex[(swapped >> shift) & 0xfu];
        }
    }
    return s;
}

inline bool eq(const HashDigest& a, const HashDigest& b) noexcept {
    return a == b;
}

} // namespace pvpgn::client_v3::hash

#endif // PVPGN_V3_CLIENT_BNCLIENT_HASH_HPP
