// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file init_codec.hpp
/// Pure-C++ wire codec for `ClientInitConn` -- the single-byte
/// connection-class packet every PvPGN client sends as its very
/// first byte after TCP connect.
///
/// The legacy parsing path reads this byte inside
/// `src/bnetd/handle_init.cpp` via the `bn_byte_get` macro on a
/// PACKED struct alias. This codec replaces that inline pattern
/// with a pure-C++ parse / encode pair so the packet pump (when
/// it lands) can dispatch off `ClientInitConn{}` values
/// instead of touching `t_client_initconn`.

#include "protocol/bnet/init_wire_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pvpgn::protocol::bnet::init {

/// Wire size of the init-conn packet (one cclass byte).
inline constexpr std::size_t kClientInitConnSize = 1;

/// Parse a `ClientInitConn` from a byte view. Returns `std::nullopt`
/// for any input that isn't exactly one byte long. The function
/// does NOT validate the cclass value -- a parser only enforces
/// framing, never policy (rate-limit / realm-list gate / class
/// dispatch live in `application/init/init_conn_dispatch`).
constexpr std::optional<ClientInitConn>
parse_client_initconn(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() != kClientInitConnSize) return std::nullopt;
    return ClientInitConn{ static_cast<std::uint8_t>(bytes[0]) };
}

/// Convenience overload for `std::span<const std::uint8_t>` so
/// callers reading from legacy `unsigned char*` buffers don't have
/// to reinterpret-cast.
constexpr std::optional<ClientInitConn>
parse_client_initconn(std::span<const std::uint8_t> bytes) noexcept
{
    if (bytes.size() != kClientInitConnSize) return std::nullopt;
    return ClientInitConn{ bytes[0] };
}

/// Encode a `ClientInitConn` to its single wire byte. Total wire
/// size is always `kClientInitConnSize`.
constexpr std::array<std::byte, kClientInitConnSize>
encode_client_initconn(ClientInitConn p) noexcept
{
    return { static_cast<std::byte>(p.cclass) };
}

}  // namespace pvpgn::protocol::bnet::init
