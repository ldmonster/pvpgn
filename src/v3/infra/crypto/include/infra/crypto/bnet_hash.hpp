// SPDX-License-Identifier: GPL-2.0-or-later
//
// infra/crypto/bnet_hash.hpp -- C++20 reimplementation of the
// Blizzard "broken SHA-1" hash and the related true-SHA-1 helpers
// that Battle.net uses on the wire.
//
// Replaces legacy src/common/bnethash.{h,cpp}. The legacy function
// signatures took (out*, size, void*) and returned int; this module
// uses std::span / std::array / std::string for a safer C++ API.
//
// Algorithm preserved bit-for-bit from the legacy implementation so
// the two implementations produce identical digests for identical
// inputs. The parity is checked at integration time (Step 8).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace pvpgn::v3::infra::crypto {

// Battle.net hashes are five little-endian 32-bit words = 20 bytes.
using BnetDigest = std::array<std::uint32_t, 5>;

// Blizzard's modified SHA-1 (with the well-known `ROTL32(1, x)` typo
// in the message schedule). Used for Battle.net legacy password and
// session hashes.
[[nodiscard]] BnetDigest blizzard_hash(std::span<const std::byte> data) noexcept;

// True SHA-1 over `data`, output words in big-endian bit order
// (canonical SHA-1 output, i.e. `H0H1H2H3H4`).
[[nodiscard]] BnetDigest sha1(std::span<const std::byte> data) noexcept;

// True SHA-1 with each output word byte-swapped (little-endian word
// layout). Matches the legacy `little_endian_sha1_hash`.
[[nodiscard]] BnetDigest sha1_le(std::span<const std::byte> data) noexcept;

// Convenience overload: hash a string-like view's underlying bytes.
[[nodiscard]] inline BnetDigest blizzard_hash(std::string_view text) noexcept
{
    return blizzard_hash(
        std::span{reinterpret_cast<const std::byte*>(text.data()), text.size()});
}

[[nodiscard]] inline BnetDigest sha1(std::string_view text) noexcept
{
    return sha1(
        std::span{reinterpret_cast<const std::byte*>(text.data()), text.size()});
}

// Format a digest as a 40-character lowercase hex string. Each word
// is rendered in host byte order (matches legacy `hash_get_str`).
[[nodiscard]] std::string to_hex(const BnetDigest& digest);

// Format a digest as a 40-character lowercase hex string with each
// word byte-reversed (matches legacy `little_endian_hash_get_str`).
[[nodiscard]] std::string to_hex_le(const BnetDigest& digest);

// Parse a 40-character hex string into a digest. Returns std::nullopt
// on length mismatch or non-hex characters.
[[nodiscard]] std::optional<BnetDigest> from_hex(std::string_view text) noexcept;

}  // namespace pvpgn::v3::infra::crypto
