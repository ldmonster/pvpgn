// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file random_source.hpp
/// Port for cryptographically-strong random byte/integer generation.

#include <cstddef>
#include <cstdint>
#include <span>

namespace pvpgn::application::ports {

class IRandomSource {
public:
    virtual ~IRandomSource() = default;

    /// Return a uniformly-distributed 32-bit value in [min, max].
    virtual std::uint32_t
        next_uint(std::uint32_t min, std::uint32_t max) = 0;

    /// Fill `buf` with random bytes.
    virtual void fill_bytes(std::span<std::byte> buf) = 0;
};

} // namespace pvpgn::application::ports
