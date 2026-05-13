// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bytes.hpp
/// Span aliases over `std::byte`, and hex encode/decode helpers.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"

namespace pvpgn::core {

using ByteSpan = std::span<std::byte>;        ///< mutable
using ByteView = std::span<const std::byte>;  ///< immutable

/// Reinterpret a contiguous range of any trivially-copyable byte-like type
/// as `ByteView`. Lifetime is the caller's responsibility.
template <class T>
constexpr ByteView as_byte_view(const T* p, std::size_t n) noexcept {
    static_assert(sizeof(T) == 1, "as_byte_view requires byte-sized elements");
    return ByteView{reinterpret_cast<const std::byte*>(p), n};
}

inline ByteView as_byte_view(std::string_view s) noexcept {
    return as_byte_view(s.data(), s.size());
}

/// Lowercase hex, no separators. Allocates.
std::string to_hex(ByteView bytes);

/// Parse `[0-9a-fA-F]` hex. Returns InvalidArgument on bad chars / odd length.
Result<std::vector<std::byte>> from_hex(std::string_view hex);

// --- inline implementations ----------------------------------------------

inline std::string to_hex(ByteView bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.resize(bytes.size() * 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto v = static_cast<std::uint8_t>(bytes[i]);
        out[2 * i]     = kDigits[v >> 4];
        out[2 * i + 1] = kDigits[v & 0x0f];
    }
    return out;
}

namespace detail {
constexpr int hex_nibble(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}
}  // namespace detail

inline Result<std::vector<std::byte>> from_hex(std::string_view hex) {
    if (hex.size() % 2 != 0)
        return fail(make_error(StatusCode::InvalidArgument, "odd-length hex"));
    std::vector<std::byte> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = detail::hex_nibble(hex[i]);
        const int lo = detail::hex_nibble(hex[i + 1]);
        if (hi < 0 || lo < 0)
            return fail(make_error(StatusCode::InvalidArgument, "bad hex char"));
        out.push_back(static_cast<std::byte>((hi << 4) | lo));
    }
    return out;
}

}  // namespace pvpgn::core
