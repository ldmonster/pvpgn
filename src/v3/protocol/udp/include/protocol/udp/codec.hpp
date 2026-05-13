// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure codec for the legacy UDP chat-side keepalive packets used by
/// StarCraft / Brood War / Diablo II clients.
///
/// Wire format (all LE):
///   u32 type   ─┐
///   (variant payload)
///
/// Known types are mirrored from `src/common/udp_protocol.h`:
///   * 0x05 SERVER_UDPTEST           — server → client; u32 bnettag ('tenb')
///   * 0x07 CLIENT_UDPPING           — client → server; u32 cookie
///   * 0x08 CLIENT_SESSIONADDR1      — client; u32 session key
///   * 0x09 CLIENT_SESSIONADDR2      — client; u32 key + u32 session number
///
/// This is a connection-less protocol; there is no FSM. Each datagram
/// decodes independently.

#include <cstdint>
#include <string>
#include <variant>

#include "core/result.hpp"
#include "core/bytes.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::udp {

inline constexpr std::uint32_t kServerUdpTest        = 0x05u;
inline constexpr std::uint32_t kClientUdpPing        = 0x07u;
inline constexpr std::uint32_t kClientSessionAddr1   = 0x08u;
inline constexpr std::uint32_t kClientSessionAddr2   = 0x09u;
inline constexpr std::uint32_t kBnetTag              = 0x626E6574u;  // 'tenb' LE

struct UdpTest {
    std::uint32_t bnettag = kBnetTag;
    bool operator==(const UdpTest&) const = default;
};
struct UdpPing {
    std::uint32_t cookie = 0;
    bool operator==(const UdpPing&) const = default;
};
struct SessionAddr1 {
    std::uint32_t session_key = 0;
    bool operator==(const SessionAddr1&) const = default;
};
struct SessionAddr2 {
    std::uint32_t session_key    = 0;
    std::uint32_t session_number = 0;
    bool operator==(const SessionAddr2&) const = default;
};

using Datagram = std::variant<UdpTest, UdpPing, SessionAddr1, SessionAddr2>;

core::Result<Datagram> decode(core::ByteView buf);
core::Status<>         encode(Writer& w, const Datagram& dg);

}  // namespace pvpgn::protocol::udp
