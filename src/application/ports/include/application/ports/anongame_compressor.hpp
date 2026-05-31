// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file anongame_compressor.hpp
/// Port abstraction for compressing anongame inforeply payloads.
/// The infra layer provides a concrete adapter (zlib).

#include <cstdint>
#include <span>
#include <vector>

#include "core/result.hpp"

namespace pvpgn::application::ports {

class IAnonGameCompressor {
public:
    virtual ~IAnonGameCompressor() = default;

    [[nodiscard]] virtual core::Result<std::vector<std::uint8_t>>
    compress(std::span<const std::uint8_t> raw) const = 0;
};

}  // namespace pvpgn::application::ports
