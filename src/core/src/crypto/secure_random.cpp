// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/crypto/secure_random.hpp"

#include <limits>
#include <random>

// When the build is configured with libsodium, prefer randombytes_buf — it is
// purpose-built, fork-safe, and avoids any libstdc++ random_device quirks.
#if defined(PVPGN_V3_WITH_SODIUM)
#  include <sodium.h>
#endif

namespace pvpgn::core::crypto {

namespace {

/// Draw `n` raw bytes from the OS entropy source into `out`.
void os_random_bytes(std::byte* out, std::size_t n, std::random_device& rd) {
#if defined(PVPGN_V3_WITH_SODIUM)
    (void)rd;
    ::randombytes_buf(out, n);
#else
    // std::random_device yields one unsigned int (>= 32 bits) of OS entropy
    // per call. Pack it out byte-by-byte.
    using word = std::random_device::result_type;
    static_assert(sizeof(word) >= 4, "random_device word must be >= 32 bits");
    std::size_t i = 0;
    while (i < n) {
        word w = rd();
        for (std::size_t b = 0; b < sizeof(word) && i < n; ++b, ++i) {
            out[i] = static_cast<std::byte>((w >> (8 * b)) & 0xFFu);
        }
    }
#endif
}

}  // namespace

struct SecureRandom::Impl {
    std::random_device rd;
};

SecureRandom::SecureRandom() : impl_(new Impl) {}

SecureRandom::SecureRandom(SecureRandom&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

SecureRandom& SecureRandom::operator=(SecureRandom&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_       = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

SecureRandom::~SecureRandom() { delete impl_; }

void SecureRandom::fill_bytes(std::span<std::byte> buf) {
    if (buf.empty()) return;
    os_random_bytes(buf.data(), buf.size(), impl_->rd);
}

std::uint32_t SecureRandom::next_u32() {
    std::uint32_t v{};
    fill_bytes(std::as_writable_bytes(std::span<std::uint32_t, 1>{&v, 1}));
    return v;
}

std::uint64_t SecureRandom::next_u64() {
    std::uint64_t v{};
    fill_bytes(std::as_writable_bytes(std::span<std::uint64_t, 1>{&v, 1}));
    return v;
}

std::uint32_t SecureRandom::uniform(std::uint32_t min, std::uint32_t max) {
    // Precondition: min <= max. Inclusive range.
    const std::uint32_t span = max - min;
    if (span == 0) return min;

    // Rejection sampling to avoid modulo bias. `limit` is the largest multiple
    // of `range` that fits in uint32; values at or above it are rejected.
    const std::uint64_t range = static_cast<std::uint64_t>(span) + 1;
    const std::uint64_t limit =
        (static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1)
        / range * range;
    std::uint32_t v;
    do {
        v = next_u32();
    } while (static_cast<std::uint64_t>(v) >= limit);
    return min + static_cast<std::uint32_t>(v % range);
}

void secure_random_bytes(std::span<std::byte> buf) {
#if defined(PVPGN_V3_WITH_SODIUM)
    if (!buf.empty()) ::randombytes_buf(buf.data(), buf.size());
#else
    thread_local std::random_device rd;
    if (!buf.empty()) os_random_bytes(buf.data(), buf.size(), rd);
#endif
}

}  // namespace pvpgn::core::crypto
