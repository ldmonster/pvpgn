// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection_fsm_internal.hpp
/// Private helpers shared across the connection_fsm_*.cpp translation units.
///
/// This header is intentionally NOT installed in the public include tree.
/// It lives alongside the .cpp files in src/ and is only visible to the
/// connection library's own translation units.
///
/// Contents:
///   - write_le32 / read_le32 / read_cstring  — packet serialisation helpers
///   - build_chat_event                        — SID_CHATEVENT body builder
///   - EID_* / kServer* constants              — chat event constants

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::connection::detail {

// ---------------------------------------------------------------------------
// Packet serialisation helpers
// ---------------------------------------------------------------------------

/// Append a little-endian uint32 to a byte buffer.
inline void write_le32(std::vector<std::byte>& buf, std::uint32_t v) {
    buf.push_back(std::byte{static_cast<std::uint8_t>( v        & 0xFFu)});
    buf.push_back(std::byte{static_cast<std::uint8_t>((v >>  8) & 0xFFu)});
    buf.push_back(std::byte{static_cast<std::uint8_t>((v >> 16) & 0xFFu)});
    buf.push_back(std::byte{static_cast<std::uint8_t>((v >> 24) & 0xFFu)});
}

/// Read a little-endian uint32 from a span at offset (returns 0 if OOB).
[[nodiscard]] inline std::uint32_t read_le32(std::span<const std::byte> s,
                                              std::size_t offset) noexcept {
    if (offset + 4 > s.size()) return 0u;
    return static_cast<std::uint32_t>(s[offset])
         | (static_cast<std::uint32_t>(s[offset + 1]) <<  8)
         | (static_cast<std::uint32_t>(s[offset + 2]) << 16)
         | (static_cast<std::uint32_t>(s[offset + 3]) << 24);
}

/// Read a null-terminated C-string from a span at offset.
/// Returns empty string if offset is out of bounds.
[[nodiscard]] inline std::string read_cstring(std::span<const std::byte> s,
                                               std::size_t offset) {
    if (offset >= s.size()) return {};
    const auto* begin = reinterpret_cast<const char*>(s.data() + offset);
    const auto* end   = reinterpret_cast<const char*>(s.data() + s.size());
    const auto* nul   = static_cast<const char*>(
        std::memchr(begin, '\0', static_cast<std::size_t>(end - begin)));
    if (!nul) return std::string{begin, end};
    return std::string{begin, nul};
}

// ---------------------------------------------------------------------------
// SID_CHATEVENT (0x0F) constants
// ---------------------------------------------------------------------------

/// Server-side sentinel values for ip/account/registration fields.
inline constexpr std::uint32_t kServerIp         = 0x00000000u;
inline constexpr std::uint32_t kServerAcctNumber = 0xBADC0FFEu;
inline constexpr std::uint32_t kServerRegAuth    = 0xBADC0FFEu;

/// SID_CHATEVENT event IDs (EID_* from legacy bnet_protocol.h).
inline constexpr std::uint32_t kEidShowUser = 0x01u;  ///< EID_SHOWUSER  — existing member on join
inline constexpr std::uint32_t kEidJoin     = 0x02u;  ///< EID_JOIN      — member joined
inline constexpr std::uint32_t kEidLeave    = 0x03u;  ///< EID_LEAVE     — member left
inline constexpr std::uint32_t kEidTalk     = 0x05u;  ///< EID_TALK      — chat message
inline constexpr std::uint32_t kEidChannel  = 0x07u;  ///< EID_CHANNEL   — channel name notification
inline constexpr std::uint32_t kEidInfo     = 0x12u;  ///< EID_INFO      — informational text
inline constexpr std::uint32_t kEidError    = 0x13u;  ///< EID_ERROR     — error text
inline constexpr std::uint32_t kEidEmote    = 0x17u;  ///< EID_EMOTE     — /me emote

// ---------------------------------------------------------------------------
// SID_CHATEVENT body builder
// ---------------------------------------------------------------------------

/// Build a raw SID_CHATEVENT body (without the 4-byte BNCS header).
///
/// SID_CHATEVENT wire layout (all LE):
///   uint32  event_id
///   uint32  user_flags
///   uint32  ping_ms
///   uint32  ip_address        (0x00000000 for server-generated events)
///   uint32  account_number    (0xBADC0FFE for server-generated events)
///   uint32  registration_auth (0xBADC0FFE for server-generated events)
///   char[]  username          (NUL-terminated)
///   char[]  text              (NUL-terminated)
[[nodiscard]] inline std::vector<std::byte> build_chat_event(
    std::uint32_t event_id,
    std::uint32_t flags,
    std::uint32_t ping_ms,
    std::string_view username,
    std::string_view text)
{
    std::vector<std::byte> body;
    body.reserve(24 + username.size() + 1 + text.size() + 1);

    auto push_le32 = [&](std::uint32_t v) {
        body.push_back(std::byte{static_cast<std::uint8_t>( v        & 0xFFu)});
        body.push_back(std::byte{static_cast<std::uint8_t>((v >>  8) & 0xFFu)});
        body.push_back(std::byte{static_cast<std::uint8_t>((v >> 16) & 0xFFu)});
        body.push_back(std::byte{static_cast<std::uint8_t>((v >> 24) & 0xFFu)});
    };

    push_le32(event_id);
    push_le32(flags);
    push_le32(ping_ms);
    push_le32(kServerIp);
    push_le32(kServerAcctNumber);
    push_le32(kServerRegAuth);

    for (char c : username) body.push_back(std::byte{static_cast<std::uint8_t>(c)});
    body.push_back(std::byte{0}); // NUL

    for (char c : text) body.push_back(std::byte{static_cast<std::uint8_t>(c)});
    body.push_back(std::byte{0}); // NUL

    return body;
}

}  // namespace pvpgn::application::connection::detail
