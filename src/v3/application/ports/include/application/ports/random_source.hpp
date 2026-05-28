// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file random_source.hpp
/// Port: cryptographically-suitable random number / byte source.

#include <cstddef>
#include <cstdint>
#include <span>

namespace pvpgn::application::ports {

/// Port: random source interface for hexagonal architecture.
/// Implementations may wrap std::mt19937, OS entropy (/dev/urandom), etc.
class IRandomSource {
public:
    virtual ~IRandomSource() = default;

    /// Return a uniformly distributed integer in the closed interval [min, max].
    virtual std::uint32_t
    next_uint(std::uint32_t min, std::uint32_t max) = 0;

    /// Fill `buf` with random bytes.
    virtual void
    fill_bytes(std::span<std::byte> buf) = 0;
};

}  // namespace pvpgn::application::ports
