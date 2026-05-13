// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bn_hash.hpp
/// `BNHash` — Battle.net password hash. Always 20 bytes (5 × u32), the
/// output of the legacy "Broken-SHA-1" implementation in
/// `src/common/bnethash.c`. Modelled as an immutable byte sequence;
/// hashing/comparison live elsewhere.

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::domain {

class BNHash {
public:
    static constexpr std::size_t kSize = 20;
    using Bytes = std::array<std::uint8_t, kSize>;

    constexpr BNHash() = default;
    constexpr explicit BNHash(Bytes b) : v_(b) {}

    /// Build from 20 raw bytes — used by persistence + protocol decode.
    static core::Result<BNHash> from_bytes(std::string_view raw) {
        if (raw.size() != kSize) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "BNHash requires exactly 20 bytes"});
        }
        Bytes b{};
        std::memcpy(b.data(), raw.data(), kSize);
        return BNHash{b};
    }

    constexpr const Bytes& bytes() const noexcept { return v_; }

    /// Constant-time equality — important because this value is
    /// compared against attacker-controlled input on every login.
    bool equals_constant_time(const BNHash& other) const noexcept {
        std::uint32_t diff = 0;
        for (std::size_t i = 0; i < kSize; ++i) {
            diff |= static_cast<std::uint32_t>(v_[i] ^ other.v_[i]);
        }
        return diff == 0;
    }

    bool operator==(const BNHash& other) const noexcept {
        return equals_constant_time(other);
    }
    bool operator!=(const BNHash& other) const noexcept { return !(*this == other); }

private:
    Bytes v_{};
};

}  // namespace pvpgn::domain
