// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compression adapter for the legacy FINDANONGAME INFOREPLY payloads.
//
// On the wire the legacy server frames each per-tag payload as:
//
//     u16 LE  raw_len           (size of the uncompressed bytes)
//     u16 LE  comp_len          (size of the deflate stream that follows)
//     bytes[comp_len]           (zlib-wrapped deflate stream of the payload)
//
// This adapter lives in `infra/compression/` because it brings in a
// real external dependency (zlib). The v3 protocol layer remains
// zlib-free and operates on decompressed bytes only — callers stitch
// the two together at the transport edge.
//
// Reference: src/bnetd/anongame_infos.cpp::zlib_compress().

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::compression {

/// Number of bytes in the legacy header (u16 raw_len + u16 comp_len).
constexpr std::size_t kAnonGameHeaderSize = 4;

/// Compress `raw` using zlib at level 9 and prepend the 4-byte legacy
/// framing header. Returns the framed bytes ready for transport.
///
/// `raw.size()` and the deflated length must both fit in `std::uint16_t`,
/// matching the legacy on-wire limits. If they do not, returns
/// `core::StatusCode::OutOfRange`.
core::Result<std::vector<std::uint8_t>> anongame_compress(
    std::span<const std::uint8_t> raw);

/// Inverse of `anongame_compress`. Validates the 4-byte header, inflates
/// the deflate stream, and checks the result length matches the header.
core::Result<std::vector<std::uint8_t>> anongame_decompress(
    std::span<const std::uint8_t> framed);

}  // namespace pvpgn::infra::compression
