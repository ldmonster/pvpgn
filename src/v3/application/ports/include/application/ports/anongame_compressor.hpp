// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file anongame_compressor.hpp
/// Port for the compression scheme used by the legacy FINDANONGAME
/// INFOREPLY message. The application layer needs to compress
/// per-tag payloads before stuffing them into the wire envelope,
/// but does not depend on a concrete compressor implementation
/// (typically zlib, lives in infra).

#include <cstdint>
#include <span>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::application::ports {

/// Compresses a per-tag INFOREPLY payload into the framed bytes the
/// wire envelope carries (typically: u16 raw_len + u16 comp_len +
/// deflate stream). Concrete implementations live in infra.
class IAnonGameCompressor {
public:
    virtual ~IAnonGameCompressor() = default;

    /// Compress `raw` and return the framed bytes. On failure returns
    /// a `core::Error` (most commonly `OutOfRange` when `raw.size()`
    /// or the compressed length does not fit in the legacy u16
    /// header fields).
    [[nodiscard]] virtual core::Result<std::vector<std::uint8_t>>
    compress(std::span<const std::uint8_t> raw) const = 0;
};

}  // namespace pvpgn::application::ports
