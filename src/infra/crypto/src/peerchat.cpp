// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/crypto/peerchat.hpp"

#include <stdexcept>

namespace pvpgn::v3::infra::crypto {

PeerchatCipher::PeerchatCipher(std::span<const std::uint8_t> challenge,
                               std::span<const std::uint8_t> gamekey)
{
    if (challenge.size() != kPeerchatChallengeBytes) {
        throw std::invalid_argument{
            "PeerchatCipher: challenge must be 16 bytes"};
    }
    if (gamekey.size() != kPeerchatGamekeyBytes) {
        throw std::invalid_argument{
            "PeerchatCipher: gamekey must be 6 bytes"};
    }

    // Step 1: XOR challenge with the (wrapping) gamekey.
    std::array<std::uint8_t, kPeerchatChallengeBytes> mixed{};
    for (std::size_t i = 0; i < kPeerchatChallengeBytes; ++i) {
        mixed[i] = static_cast<std::uint8_t>(
            challenge[i] ^ gamekey[i % kPeerchatGamekeyBytes]);
    }

    // Step 2: state initialised as 255..0 (descending), exactly as
    // legacy `do { *p1++ = t1--; } while (p1 != l1);` with t1
    // starting at 255 and `l1 = crypt + 256`.
    for (std::size_t i = 0; i < kPeerchatStateBytes; ++i) {
        state_[i] = static_cast<std::uint8_t>(255u - i);
    }

    // Step 3: KSA-style shuffle. The mixed challenge is read with
    // wrap-around. `t1` accumulates as a uint8_t (implicit modulo
    // 256) -- matches legacy unsigned-char arithmetic.
    std::uint8_t t1 = 0;
    for (std::size_t i = 0; i < kPeerchatStateBytes; ++i) {
        const std::uint8_t cb = mixed[i % kPeerchatChallengeBytes];
        t1 = static_cast<std::uint8_t>(t1 + cb + state_[i]);
        std::swap(state_[t1], state_[i]);
    }

    n1_ = 0;
    n2_ = 0;
}

void PeerchatCipher::transform(std::span<std::uint8_t> data)
{
    std::uint8_t num1 = n1_;
    std::uint8_t num2 = n2_;
    for (auto& byte : data) {
        ++num1;
        const std::uint8_t t = state_[num1];
        num2 = static_cast<std::uint8_t>(num2 + t);
        state_[num1] = state_[num2];
        state_[num2] = t;
        const std::uint8_t mix =
            static_cast<std::uint8_t>(t + state_[num1]);
        byte ^= state_[mix];
    }
    n1_ = num1;
    n2_ = num2;
}

}  // namespace pvpgn::v3::infra::crypto
