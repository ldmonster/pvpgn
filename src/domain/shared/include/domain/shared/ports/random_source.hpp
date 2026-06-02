// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file random_source.hpp
/// Application-layer port for a source of randomness.
///
/// Abstracts random number / byte generation so the application layer can be
/// driven by a deterministic, seedable source in tests and a CSPRNG in
/// production.

#include <cstddef>
#include <cstdint>
#include <span>

namespace pvpgn::application::ports {

/// Supplies random integers and bytes.
class IRandomSource {
public:
    virtual ~IRandomSource() = default;

    /// Return a uniformly distributed integer in [min, max].
    virtual std::uint32_t next_uint(std::uint32_t min, std::uint32_t max) = 0;

    /// Fill `buf` with random bytes.
    virtual void fill_bytes(std::span<std::byte> buf) = 0;
};

}  // namespace pvpgn::application::ports
