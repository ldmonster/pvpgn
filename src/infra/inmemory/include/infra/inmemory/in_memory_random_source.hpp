// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_random_source.hpp
/// In-memory fake for IRandomSource backed by std::mt19937.
/// Suitable for tests and development/CI composition roots.

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>
#include <span>

#include "application/ports/random_source.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryRandomSource final
    : public application::ports::IRandomSource {
public:
    InMemoryRandomSource()
        : rng_(std::random_device{}()) {}

    explicit InMemoryRandomSource(std::uint32_t seed)
        : rng_(seed) {}

    std::uint32_t
    next_uint(std::uint32_t min, std::uint32_t max) override {
        std::unique_lock lock(mutex_);
        std::uniform_int_distribution<std::uint32_t> dist(min, max);
        return dist(rng_);
    }

    void
    fill_bytes(std::span<std::byte> buf) override {
        std::unique_lock lock(mutex_);
        std::uniform_int_distribution<std::uint32_t> dist(0, 255);
        for (auto& b : buf) {
            b = static_cast<std::byte>(dist(rng_));
        }
    }

private:
    std::mutex mutex_;
    std::mt19937 rng_;
};

}  // namespace pvpgn::infra::inmemory
