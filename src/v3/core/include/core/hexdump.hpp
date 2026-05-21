// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file hexdump.hpp
/// Debug hex-dump utilities — pure C++20 replacement for
/// `src/common/hexdump.cpp/.h`.
///
/// Two entry points:
///   - `hexdump_line(data, len, offset)` — format a single 16-byte row into
///     a `std::string` (address | hex bytes | ASCII).
///   - `hexdump(data, len)` — format an entire buffer as a multi-line string.
///
/// No dependency on `<cstdio>`, `eventlog`, or any legacy header.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace pvpgn::core {

namespace detail {

/// Hex nibble lookup table.
inline constexpr std::array<char, 16> kHexChars{
    '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};

/// Append a two-digit hex representation of `byte` to `out`.
inline void append_hex_byte(std::string& out, std::uint8_t byte) {
    out += kHexChars[byte >> 4];
    out += kHexChars[byte & 0x0f];
}

}  // namespace detail

/// Format a single 16-byte (or shorter) row in the classic hexdump style:
///
///   OOOO:   XX XX XX XX XX XX XX XX   XX XX XX XX XX XX XX XX   ................
///
/// where `OOOO` is the byte offset of the first byte in this row (decimal,
/// 4 digits), `XX` are hex bytes, and the trailing column shows printable
/// ASCII (`.` for non-printable).
///
/// @param data    Pointer to the start of the row (may be fewer than 16 bytes).
/// @param len     Number of bytes in this row (1–16).
/// @param offset  Byte offset of `data[0]` within the full buffer.
[[nodiscard]] inline std::string
hexdump_line(const std::uint8_t* data, std::size_t len, std::size_t offset) {
    std::string out;
    out.reserve(80);

    // Address column: 4 hex digits + ":   "
    out += detail::kHexChars[(offset >> 12) & 0xf];
    out += detail::kHexChars[(offset >>  8) & 0xf];
    out += detail::kHexChars[(offset >>  4) & 0xf];
    out += detail::kHexChars[(offset      ) & 0xf];
    out += ':';
    out += ' ';
    out += ' ';
    out += ' ';

    // Left hex half (bytes 0–7)
    for (std::size_t i = 0; i < 8; ++i) {
        if (i < len) {
            detail::append_hex_byte(out, data[i]);
            out += ' ';
        } else {
            out += "   ";
        }
    }

    out += ' ';
    out += ' ';

    // Right hex half (bytes 8–15)
    for (std::size_t i = 8; i < 16; ++i) {
        if (i < len) {
            detail::append_hex_byte(out, data[i]);
            out += ' ';
        } else {
            out += "   ";
        }
    }

    out += ' ';
    out += ' ';
    out += ' ';

    // ASCII column
    for (std::size_t i = 0; i < len; ++i) {
        const auto b = data[i];
        out += (b >= 32 && b < 127) ? static_cast<char>(b) : '.';
    }

    return out;
}

/// Overload accepting a `std::span<const std::byte>`.
[[nodiscard]] inline std::string
hexdump_line(std::span<const std::byte> row, std::size_t offset) {
    return hexdump_line(
        reinterpret_cast<const std::uint8_t*>(row.data()),
        row.size(),
        offset);
}

/// Format an entire buffer as a multi-line hexdump string.
/// Each line is terminated with `'\n'`.
///
/// @param data  Pointer to the buffer start.
/// @param len   Total number of bytes to dump.
[[nodiscard]] inline std::string
hexdump(const void* data, std::size_t len) {
    if (!data || len == 0) return {};

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::string result;
    result.reserve((len / 16 + 1) * 80);

    for (std::size_t offset = 0; offset < len; offset += 16) {
        const std::size_t row_len = (len - offset < 16) ? (len - offset) : 16;
        result += hexdump_line(bytes + offset, row_len, offset);
        result += '\n';
    }

    return result;
}

/// Overload accepting a `std::span<const std::byte>`.
[[nodiscard]] inline std::string
hexdump(std::span<const std::byte> buf) {
    return hexdump(buf.data(), buf.size());
}

}  // namespace pvpgn::core
