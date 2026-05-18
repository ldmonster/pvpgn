// SPDX-License-Identifier: GPL-2.0-or-later
//
// infra/crypto/bnet_hash_conv.hpp -- byte-order conversion between a
// host-order `BnetDigest` (5 x uint32_t) and the 20-byte Battle.net
// wire form (5 little-endian 32-bit words). Replaces legacy
// src/common/bnethashconv.{h,cpp} (`bnhash_to_hash` / `hash_to_bnhash`).

#pragma once

#include "infra/crypto/bnet_hash.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pvpgn::v3::infra::crypto {

using BnetWireHash = std::array<std::uint8_t, 20>;

// Deserialize 20 little-endian wire bytes into a host-order digest.
[[nodiscard]] BnetDigest digest_from_wire(std::span<const std::byte, 20> wire) noexcept;
[[nodiscard]] BnetDigest digest_from_wire(const BnetWireHash& wire) noexcept;

// Serialize a host-order digest into 20 little-endian wire bytes.
[[nodiscard]] BnetWireHash digest_to_wire(const BnetDigest& digest) noexcept;
void                       digest_to_wire(const BnetDigest& digest,
                                          std::span<std::byte, 20> out) noexcept;

}  // namespace pvpgn::v3::infra::crypto
