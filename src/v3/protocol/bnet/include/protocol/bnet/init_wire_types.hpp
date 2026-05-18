// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file init_wire_types.hpp
/// Wire types for the single-byte "initial connection class" packet
/// every PvPGN client sends as the first byte after TCP connect.
///
/// Mirrors `src/common/init_protocol.h` (legacy `t_client_initconn`)
/// without the legacy `bn_*` macros or PACK trickery; a `cclass`
/// byte is just a byte.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::bnet::init {

/// Single-byte connection class sent by clients on first connect.
struct ClientInitConn
{
    std::uint8_t cclass = 0;

    constexpr bool operator==(const ClientInitConn&) const = default;
};

static_assert(std::is_trivially_copyable_v<ClientInitConn>);
static_assert(sizeof(ClientInitConn) == 1,
              "ClientInitConn must be a single wire byte");

/// Known `cclass` values from `init_protocol.h`. Several services
/// share the listener at the byte-1 layer; the value uniquely
/// dispatches to the correct service-side codec.
inline constexpr std::uint8_t kClassBnet         = 0x01;   ///< Battle.net session.
inline constexpr std::uint8_t kClassFile         = 0x02;   ///< BNFTP file transfer.
inline constexpr std::uint8_t kClassBot          = 0x03;   ///< Bot interface.
inline constexpr std::uint8_t kClassEnc          = 0x04;   ///< Encrypted connection.
inline constexpr std::uint8_t kClassTelnet       = 0x0d;   ///< Telnet (look for CR).
inline constexpr std::uint8_t kClassD2cs         = 0x01;   ///< D2CS (shares value with Bnet on a different listener).
inline constexpr std::uint8_t kClassD2gs         = 0x64;
inline constexpr std::uint8_t kClassD2csBnetd    = 0x65;
inline constexpr std::uint8_t kClassLocalMachine = 0x98;   ///< Local (127.0.0.1) connection.

}  // namespace pvpgn::protocol::bnet::init
