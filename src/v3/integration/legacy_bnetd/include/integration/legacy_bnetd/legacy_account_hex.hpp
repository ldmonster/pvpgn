// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_account_hex.hpp
/// Tiny hex codec used by the legacy account repository to round-trip
/// `passhash1` between the legacy 40-char hex attribute and the
/// domain's 20-byte `BNHash`. Lives in its own header so unit tests
/// can exercise it without dragging in any legacy globals.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "domain/shared/bn_hash.hpp"

namespace pvpgn::integration::legacy_bnetd::detail {

constexpr std::optional<std::uint8_t> hex_nibble(char c) noexcept {
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
    return std::nullopt;
}

inline std::optional<domain::BNHash::Bytes>
hex_decode_passhash(std::string_view hex) noexcept {
    if (hex.size() != domain::BNHash::kSize * 2) return std::nullopt;
    domain::BNHash::Bytes out{};
    for (std::size_t i = 0; i < domain::BNHash::kSize; ++i) {
        auto hi = hex_nibble(hex[i * 2]);
        auto lo = hex_nibble(hex[i * 2 + 1]);
        if (!hi || !lo) return std::nullopt;
        out[i] = static_cast<std::uint8_t>((*hi << 4) | *lo);
    }
    return out;
}

inline std::string hex_encode_passhash(const domain::BNHash& hash) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.resize(domain::BNHash::kSize * 2);
    const auto& bytes = hash.bytes();
    for (std::size_t i = 0; i < domain::BNHash::kSize; ++i) {
        out[i * 2]     = kDigits[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = kDigits[bytes[i] & 0xF];
    }
    return out;
}

}  // namespace pvpgn::integration::legacy_bnetd::detail
