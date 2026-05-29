// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string_view>

namespace pvpgn::protocol::common {

/// Taxonomy of errors that can occur during packet decoding.
enum class DecodeError : std::uint8_t {
    Truncated,           ///< Buffer ended before the full frame was available
    UnknownOpcode,       ///< Opcode not recognised by this codec
    MalformedString,     ///< Null-terminator missing or string exceeds buffer
    InvalidLength,       ///< Declared length field is inconsistent with buffer size
    UnsupportedVersion,  ///< Protocol version not supported by this codec
    ChecksumMismatch,    ///< Packet checksum does not match computed value
};

/// Returns a human-readable description of a DecodeError.
[[nodiscard]] constexpr std::string_view to_string(DecodeError e) noexcept {
    switch (e) {
        case DecodeError::Truncated:           return "truncated";
        case DecodeError::UnknownOpcode:       return "unknown opcode";
        case DecodeError::MalformedString:     return "malformed string";
        case DecodeError::InvalidLength:       return "invalid length";
        case DecodeError::UnsupportedVersion:  return "unsupported version";
        case DecodeError::ChecksumMismatch:    return "checksum mismatch";
    }
    return "unknown";
}

} // namespace pvpgn::protocol::common
