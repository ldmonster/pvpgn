// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file zlib_anongame_compressor.hpp
/// Concrete `domain::matchmaking::IAnonGameCompressor` adapter that
/// delegates to the zlib-backed `anongame_compress` free function in
/// this same module. Lives in infra because zlib is a heavyweight
/// external dependency that the application layer must not know
/// about.

#include "domain/matchmaking/ports.hpp"
#include "infra/compression/zlib_anongame.hpp"


namespace pvpgn::infra::compression {

class ZlibAnonGameCompressor final
    : public domain::matchmaking::IAnonGameCompressor {
public:
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    compress(std::span<const std::uint8_t> raw) const override {
        return anongame_compress(raw);
    }
};

}  // namespace pvpgn::infra::compression
