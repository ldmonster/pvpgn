// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/matchmaking/ports.hpp — Abstract ports (interfaces) for the matchmaking bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.

#include <cstdint>
#include <span>
#include <vector>

#include "core/result.hpp"

namespace pvpgn::domain::matchmaking {

// ---------------------------------------------------------------------------
// IAnonGameCompressor
// ---------------------------------------------------------------------------

/// Port abstraction for compressing anongame inforeply payloads.
/// The infra layer provides a concrete adapter (zlib).
class IAnonGameCompressor {
public:
    virtual ~IAnonGameCompressor() = default;

    [[nodiscard]] virtual core::Result<std::vector<std::uint8_t>>
    compress(std::span<const std::uint8_t> raw) const = 0;
};

} // namespace pvpgn::domain::matchmaking
