// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// Foundation wire types for the bnet protocol family, mirrored from
/// `src/common/bnet_protocol.h`.
///
/// This file pins ONLY the foundational header structs and the universal
/// "no-op" packet code. Topic-specific packet codes live in sibling
/// sub-headers (`w3route_wire_types.hpp`, `auth_wire_types.hpp`,
/// `account_wire_types.hpp`, `chat_wire_types.hpp`, `game_wire_types.hpp`,
/// `clan_wire_types.hpp`, `misc_wire_types.hpp`, `anongame_wire_types.hpp`).
///
/// Per-message struct shapes (179 in the legacy header) are deliberately
/// deferred and land incrementally with the codec implementations. The
/// numeric wire surface is what needs pinning today; struct layout is an
/// internal implementation detail mediated by the codec layer.

#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::bnet {

/// Generic 4-byte bnet packet header: `bn_short type; bn_short size;`
/// on the wire (little-endian). Native ints here; the codec converts.
struct BnetHeader {
    std::uint16_t type{};
    std::uint16_t size{};
    bool operator==(const BnetHeader&) const = default;
};
static_assert(sizeof(BnetHeader) == 4);
static_assert(std::is_trivially_copyable_v<BnetHeader>);

/// War3 "anongame routing" header. Same shape as `BnetHeader` but
/// kept as a distinct type to preserve the legacy class distinction.
struct W3RouteHeader {
    std::uint16_t type{};
    std::uint16_t size{};
    bool operator==(const W3RouteHeader&) const = default;
};
static_assert(sizeof(W3RouteHeader) == 4);
static_assert(std::is_trivially_copyable_v<W3RouteHeader>);

/// "Generic" wrappers used for unhandled / minimal packets.
struct BnetGeneric {
    BnetHeader h{};
    bool operator==(const BnetGeneric&) const = default;
};
struct W3RouteGeneric {
    W3RouteHeader h{};
    bool operator==(const W3RouteGeneric&) const = default;
};

/// Used for unhandled pmap packets.
inline constexpr std::uint16_t kClientNull = 0xfeff;

} // namespace pvpgn::protocol::bnet
