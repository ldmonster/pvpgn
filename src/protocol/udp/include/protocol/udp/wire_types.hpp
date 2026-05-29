// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// UDP keepalive / session datagram wire types, mirrored from
/// `src/common/udp_protocol.h`. Every wire field is a little-endian
/// `uint32_t`; we represent them with native `std::uint32_t` here
/// and rely on the codec (in `codec.hpp` / `codec.cpp`) to handle
/// byte-order conversion at the read/write boundary.
///
/// These structs are NOT memcpy'd off the wire; they exist for
/// type-safe parsing/serialisation and to expose the protocol
/// constants used by the UDP codec and its consumers.

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::udp::wire {

/// All UDP datagrams begin with a u32 little-endian `type` field.
inline constexpr std::size_t kHeaderBytes = 4;

/// Known datagram type codes (also re-exported from `codec.hpp`).
inline constexpr std::uint32_t kServerUdpTest      = 0x00000005u;  ///< server -> client
inline constexpr std::uint32_t kClientUdpPing      = 0x00000007u;  ///< client -> server
inline constexpr std::uint32_t kClientSessionAddr1 = 0x00000008u;  ///< client -> server (BW <=1.04)
inline constexpr std::uint32_t kClientSessionAddr2 = 0x00000009u;  ///< client -> server (BW 1.07+)

/// `'bnet'` LE -- echoed back by the client in a UDPOK response.
inline constexpr std::uint32_t kBnetTag = 0x626E6574u;

/// Header common to every UDP packet.
struct UdpHeader
{
    std::uint32_t type = 0;
    constexpr bool operator==(const UdpHeader&) const = default;
};

/// `SERVER_UDPTEST` -- 8 bytes total.
struct ServerUdpTest
{
    UdpHeader     h{};
    std::uint32_t bnettag = kBnetTag;
    constexpr bool operator==(const ServerUdpTest&) const = default;
};

/// `CLIENT_UDPPING` -- 8 bytes total. `unknown1` was once thought
/// to be a timestamp; PvPGN historically just echoes it back as a
/// cookie.
struct ClientUdpPing
{
    UdpHeader     h{};
    std::uint32_t unknown1 = 0;
    constexpr bool operator==(const ClientUdpPing&) const = default;
};

/// `CLIENT_SESSIONADDR1` -- 8 bytes total.
struct ClientSessionAddr1
{
    UdpHeader     h{};
    std::uint32_t sessionkey = 0;
    constexpr bool operator==(const ClientSessionAddr1&) const = default;
};

/// `CLIENT_SESSIONADDR2` -- 12 bytes total.
struct ClientSessionAddr2
{
    UdpHeader     h{};
    std::uint32_t sessionkey = 0;
    std::uint32_t sessionnum = 0;
    constexpr bool operator==(const ClientSessionAddr2&) const = default;
};

static_assert(std::is_trivially_copyable_v<UdpHeader>);
static_assert(std::is_trivially_copyable_v<ServerUdpTest>);
static_assert(std::is_trivially_copyable_v<ClientUdpPing>);
static_assert(std::is_trivially_copyable_v<ClientSessionAddr1>);
static_assert(std::is_trivially_copyable_v<ClientSessionAddr2>);

// NOTE: We deliberately do NOT `static_assert(sizeof(...) == N)` on
// these structs because their natural alignment on common ABIs is
// 4-byte (already matching the wire layout for this protocol), but
// the v3 codec does not rely on memcpy: it explicitly reads and
// writes each field through `Reader`/`Writer` (LE), so a hypothetical
// future struct with mixed widths would still serialize correctly.

}  // namespace pvpgn::protocol::udp::wire
