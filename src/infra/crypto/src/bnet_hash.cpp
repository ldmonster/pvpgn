// SPDX-License-Identifier: GPL-2.0-or-later
//
// C++20 reimplementation of the Blizzard "broken SHA-1" hash and
// the true SHA-1 helpers from legacy src/common/bnethash.cpp. The
// numerical algorithm is preserved exactly so that the new module
// is byte-for-byte compatible with the legacy implementation.

#include "infra/crypto/bnet_hash.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <cstring>

namespace pvpgn::v3::infra::crypto {

namespace {

enum class HashVariant { Blizzard, Sha1 };

constexpr std::uint32_t rotl32(std::uint32_t value, int amount) noexcept
{
    return std::rotl(value, amount);
}

constexpr std::uint32_t byteswap32(std::uint32_t v) noexcept
{
    return ((v & 0x000000ffu) << 24) | ((v & 0x0000ff00u) << 8)
         | ((v & 0x00ff0000u) >> 8)  | ((v & 0xff000000u) >> 24);
}

void hash_init(BnetDigest& digest) noexcept
{
    digest[0] = 0x67452301;
    digest[1] = 0xefcdab89;
    digest[2] = 0x98badcfe;
    digest[3] = 0x10325476;
    digest[4] = 0xc3d2e1f0;
}

// Process one 16-word block. Mirrors legacy `do_hash` exactly,
// including the Blizzard-vs-SHA1 message-schedule difference.
void do_hash(BnetDigest& digest, std::array<std::uint32_t, 80>& tmp,
             HashVariant variant) noexcept
{
    // Message schedule: the legacy Blizzard variant uses ROTL32(1, x)
    // (the famous bug -- the rotate amount and the value are swapped),
    // while the true-SHA-1 variant uses ROTL32(x, 1).
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t mix = tmp[i] ^ tmp[i + 8] ^ tmp[i + 2] ^ tmp[i + 13];
        tmp[i + 16] = (variant == HashVariant::Blizzard) ? rotl32(1, mix)
                                                         : rotl32(mix, 1);
    }

    std::uint32_t a = digest[0];
    std::uint32_t b = digest[1];
    std::uint32_t c = digest[2];
    std::uint32_t d = digest[3];
    std::uint32_t e = digest[4];
    std::uint32_t g = 0;

    std::size_t i = 0;

    for (; i < 20; ++i) {
        g = tmp[i] + rotl32(a, 5) + e + ((b & c) | (~b & d)) + 0x5a827999u;
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = g;
    }
    for (; i < 40; ++i) {
        g = (d ^ c ^ b) + e + rotl32(g, 5) + tmp[i] + 0x6ed9eba1u;
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = g;
    }
    for (; i < 60; ++i) {
        g = tmp[i] + rotl32(g, 5) + e + ((c & b) | (d & c) | (d & b))
          - 0x70e44324u;
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = g;
    }
    for (; i < 80; ++i) {
        g = (d ^ c ^ b) + e + rotl32(g, 5) + tmp[i] - 0x359d3e2au;
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = g;
    }

    digest[0] += g;
    digest[1] += b;
    digest[2] += c;
    digest[3] += d;
    digest[4] += e;
}

// Pack up to 64 bytes of `src` into the first 16 words of `dst` and
// zero-fill the rest. Mirrors legacy `hash_set_16`.
void hash_set_16(std::array<std::uint32_t, 80>& dst,
                 const std::uint8_t* src, std::size_t count,
                 HashVariant variant) noexcept
{
    std::size_t pos = 0;
    for (std::size_t i = 0; i < 16; ++i) {
        std::uint32_t word = 0;

        // Byte 0
        if (variant == HashVariant::Blizzard) {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]);
        } else {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]) << 24;
            else if (pos == count)
                word |= 0x80000000u;
        }
        ++pos;

        // Byte 1
        if (variant == HashVariant::Blizzard) {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]) << 8;
        } else {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]) << 16;
            else if (pos == count)
                word |= 0x800000u;
        }
        ++pos;

        // Byte 2
        if (variant == HashVariant::Blizzard) {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]) << 16;
        } else {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]) << 8;
            else if (pos == count)
                word |= 0x8000u;
        }
        ++pos;

        // Byte 3
        if (variant == HashVariant::Blizzard) {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]) << 24;
        } else {
            if (pos < count)
                word |= static_cast<std::uint32_t>(src[pos]);
            else if (pos == count)
                word |= 0x80u;
        }
        ++pos;

        dst[i] = word;
    }
}

// Append the original-message length (in bits) into words 14-15 of
// the last block. Mirrors legacy `hash_set_length`.
void hash_set_length(std::array<std::uint32_t, 80>& dst,
                     std::size_t byte_count) noexcept
{
    std::uint32_t size_high = 0;
    std::uint32_t size_low  = 0;
    for (std::size_t i = 0; i < byte_count; ++i) {
        size_low += 8;
        if (size_low == 0)
            ++size_high;
    }

    dst[14] |= ((size_high >> 24) & 0xffu) << 24;
    dst[14] |= ((size_high >> 16) & 0xffu) << 16;
    dst[14] |= ((size_high >> 8)  & 0xffu) << 8;
    dst[14] |= ((size_high)       & 0xffu);

    dst[15] |= ((size_low >> 24) & 0xffu) << 24;
    dst[15] |= ((size_low >> 16) & 0xffu) << 16;
    dst[15] |= ((size_low >> 8)  & 0xffu) << 8;
    dst[15] |= ((size_low)       & 0xffu);
}

}  // namespace

BnetDigest blizzard_hash(std::span<const std::byte> data) noexcept
{
    BnetDigest digest{};
    hash_init(digest);

    std::array<std::uint32_t, 80> tmp{};
    const auto* src  = reinterpret_cast<const std::uint8_t*>(data.data());
    std::size_t size = data.size();

    while (size > 0) {
        const std::size_t inc = (size > 64) ? 64 : size;
        hash_set_16(tmp, src, inc, HashVariant::Blizzard);
        do_hash(digest, tmp, HashVariant::Blizzard);
        src  += inc;
        size -= inc;
    }
    return digest;
}

BnetDigest sha1(std::span<const std::byte> data) noexcept
{
    BnetDigest digest{};
    hash_init(digest);

    std::array<std::uint32_t, 80> tmp{};
    const auto*       src         = reinterpret_cast<const std::uint8_t*>(data.data());
    const std::size_t orig_size   = data.size();
    std::size_t       size        = orig_size;

    // NOTE: matches legacy `sha1_hash` bug-for-bug. The legacy
    // implementation iterates only while `size > 0`, so an empty
    // input is not padded and the result equals the SHA-1 IV
    // (not the canonical SHA-1 of the empty string). Battle.net
    // never hashes an empty payload so this divergence from FIPS
    // 180-1 is irrelevant to the wire protocol.

    while (size > 0) {
        const std::size_t inc = (size >= 64) ? 64 : size;

        if (size >= 64) {
            hash_set_16(tmp, src, inc, HashVariant::Sha1);
            do_hash(digest, tmp, HashVariant::Sha1);
        } else if (size > 55) {
            // Data + 0x80 sentinel doesn't leave room for the
            // 8-byte length -- need a second padding block.
            hash_set_16(tmp, src, inc, HashVariant::Sha1);
            do_hash(digest, tmp, HashVariant::Sha1);

            hash_set_16(tmp, src, 0, HashVariant::Blizzard);
            hash_set_length(tmp, orig_size);
            do_hash(digest, tmp, HashVariant::Sha1);
        } else {
            hash_set_16(tmp, src, inc, HashVariant::Sha1);
            hash_set_length(tmp, orig_size);
            do_hash(digest, tmp, HashVariant::Sha1);
        }

        src  += inc;
        size -= inc;
    }
    return digest;
}

BnetDigest sha1_le(std::span<const std::byte> data) noexcept
{
    BnetDigest d = sha1(data);
    for (auto& word : d) {
        word = byteswap32(word);
    }
    return d;
}

std::string to_hex(const BnetDigest& digest)
{
    static constexpr char hexchars[] = "0123456789abcdef";
    std::string out(40, '0');
    for (std::size_t i = 0; i < 5; ++i) {
        const std::uint32_t w = digest[i];
        for (std::size_t nybble = 0; nybble < 8; ++nybble) {
            const auto shift = (7 - nybble) * 4;
            out[i * 8 + nybble] = hexchars[(w >> shift) & 0xfu];
        }
    }
    return out;
}

std::string to_hex_le(const BnetDigest& digest)
{
    BnetDigest swapped = digest;
    for (auto& w : swapped) w = byteswap32(w);
    return to_hex(swapped);
}

std::optional<BnetDigest> from_hex(std::string_view text) noexcept
{
    if (text.size() != 40) return std::nullopt;
    BnetDigest digest{};
    for (std::size_t i = 0; i < 5; ++i) {
        std::uint32_t  w   = 0;
        const auto*    p   = text.data() + i * 8;
        const auto*    end = p + 8;
        const auto result  = std::from_chars(p, end, w, 16);
        if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
        digest[i] = w;
    }
    return digest;
}

}  // namespace pvpgn::v3::infra::crypto
