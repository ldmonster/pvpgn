// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file secure_random.hpp
/// `core::crypto::SecureRandom` — the canonical cryptographically-secure RNG.
///
/// Plan 08 (`plans/08-crypto-modernization.md`) bans `std::rand()` in `src/`
/// and consolidates randomness on a single source. This wrapper pulls bytes
/// from the operating-system entropy pool:
///   - by default via `std::random_device`, which on the supported platforms
///     is backed by the OS CSPRNG (`getrandom(2)` / `/dev/urandom`);
///   - when built with libsodium (`PVPGN_V3_WITH_SODIUM`), via
///     `randombytes_buf`.
///
/// It is deliberately NOT seeded into a PRNG (`std::mt19937` et al. are not
/// cryptographically secure). Every call draws fresh OS entropy.
///
/// Thread-safety: `SecureRandom` owns a `std::random_device`, which is not
/// guaranteed thread-safe; give each thread its own instance, or use the
/// free `secure_random_bytes()` helper, which uses a `thread_local` engine.

#include <cstddef>
#include <cstdint>
#include <span>

namespace pvpgn::core::crypto {

/// A cryptographically-secure source of random bytes and integers.
class SecureRandom {
public:
    SecureRandom();

    // Movable, non-copyable (wraps an OS entropy handle).
    SecureRandom(SecureRandom&&) noexcept;
    SecureRandom& operator=(SecureRandom&&) noexcept;
    SecureRandom(const SecureRandom&)            = delete;
    SecureRandom& operator=(const SecureRandom&) = delete;
    ~SecureRandom();

    /// Fill `buf` with cryptographically-secure random bytes.
    void fill_bytes(std::span<std::byte> buf);

    /// A uniformly-distributed 32-bit value.
    [[nodiscard]] std::uint32_t next_u32();

    /// A uniformly-distributed 64-bit value.
    [[nodiscard]] std::uint64_t next_u64();

    /// A uniformly-distributed integer in the inclusive range [min, max].
    /// Uses rejection sampling, so the result is bias-free.
    /// Precondition: `min <= max`.
    [[nodiscard]] std::uint32_t uniform(std::uint32_t min, std::uint32_t max);

private:
    struct Impl;
    Impl* impl_;
};

/// Convenience: fill `buf` with secure random bytes using a thread-local
/// engine. Equivalent to a per-thread `SecureRandom::fill_bytes`.
void secure_random_bytes(std::span<std::byte> buf);

}  // namespace pvpgn::core::crypto
