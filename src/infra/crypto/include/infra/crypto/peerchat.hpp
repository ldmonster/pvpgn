// SPDX-License-Identifier: GPL-2.0-or-later
//
// GameSpy peerchat encryption/decryption algorithm (variant of RC4)
// as used by the legacy implementation in `src/common/peerchat.cpp`.
// Port of Luigi Auriemma's reference implementation.
//
// The cipher is symmetric (encrypt == decrypt). Both endpoints
// initialise their context with the same `challenge` (16 bytes) and
// `gamekey` (6 bytes), then apply `transform()` to each frame of
// data in order.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pvpgn::v3::infra::crypto {

inline constexpr std::size_t kPeerchatChallengeBytes = 16;
inline constexpr std::size_t kPeerchatGamekeyBytes   = 6;
inline constexpr std::size_t kPeerchatStateBytes     = 256;

class PeerchatCipher {
public:
    // Initialise the cipher state from a 16-byte challenge and a
    // 6-byte game key. Throws `std::invalid_argument` if either
    // span has the wrong size.
    PeerchatCipher(std::span<const std::uint8_t> challenge,
                   std::span<const std::uint8_t> gamekey);

    // Apply the cipher to `data` in-place. May be called any number
    // of times; state is preserved between calls.
    void transform(std::span<std::uint8_t> data);

    // Accessors for parity testing.
    [[nodiscard]] std::uint8_t counter_1() const noexcept { return n1_; }
    [[nodiscard]] std::uint8_t counter_2() const noexcept { return n2_; }
    [[nodiscard]] const std::array<std::uint8_t, kPeerchatStateBytes>&
    state() const noexcept { return state_; }

private:
    std::array<std::uint8_t, kPeerchatStateBytes> state_{};
    std::uint8_t n1_{0};
    std::uint8_t n2_{0};
};

}  // namespace pvpgn::v3::infra::crypto
