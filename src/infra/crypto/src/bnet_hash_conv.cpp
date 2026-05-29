// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/crypto/bnet_hash_conv.hpp"

#include <cstdint>

namespace pvpgn::v3::infra::crypto {

namespace {

constexpr std::uint32_t load_le32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

constexpr void store_le32(std::uint8_t* p, std::uint32_t v) noexcept
{
    p[0] = static_cast<std::uint8_t>(v & 0xffu);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
    p[2] = static_cast<std::uint8_t>((v >> 16) & 0xffu);
    p[3] = static_cast<std::uint8_t>((v >> 24) & 0xffu);
}

}  // namespace

BnetDigest digest_from_wire(std::span<const std::byte, 20> wire) noexcept
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(wire.data());
    BnetDigest d{};
    for (std::size_t i = 0; i < 5; ++i) {
        d[i] = load_le32(bytes + i * 4);
    }
    return d;
}

BnetDigest digest_from_wire(const BnetWireHash& wire) noexcept
{
    BnetDigest d{};
    for (std::size_t i = 0; i < 5; ++i) {
        d[i] = load_le32(wire.data() + i * 4);
    }
    return d;
}

BnetWireHash digest_to_wire(const BnetDigest& digest) noexcept
{
    BnetWireHash out{};
    for (std::size_t i = 0; i < 5; ++i) {
        store_le32(out.data() + i * 4, digest[i]);
    }
    return out;
}

void digest_to_wire(const BnetDigest& digest,
                    std::span<std::byte, 20> out) noexcept
{
    auto* bytes = reinterpret_cast<std::uint8_t*>(out.data());
    for (std::size_t i = 0; i < 5; ++i) {
        store_le32(bytes + i * 4, digest[i]);
    }
}

}  // namespace pvpgn::v3::infra::crypto
