// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_wire_types.hpp
/// Realm-list / realm-join wire constants (D2 realm selection),
/// mirrored from `src/common/bnet_protocol.h` (lines ~1197-1308,
/// 3584-3625). Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::realm {

// ---- Packet type codes -------------------------------------------------
inline constexpr std::uint16_t kClientRealmListReq    = 0x34ff;
inline constexpr std::uint16_t kServerRealmListReply  = 0x34ff;

inline constexpr std::uint16_t kClientRealmListReq110   = 0x40ff;
inline constexpr std::uint16_t kServerRealmListReply110 = 0x40ff;

inline constexpr std::uint16_t kClientRealmJoinReq109   = 0x3eff;
inline constexpr std::uint16_t kServerRealmJoinReply109 = 0x3eff;

// ---- RealmListReply payload magic constants ----------------------------
inline constexpr std::uint32_t kRealmListReplyUnknown1     = 0x00000000;
inline constexpr std::uint32_t kRealmListReplyDataUnknown3 = 0xc0000000;
inline constexpr std::uint32_t kRealmListReplyDataUnknown4 = 0x00000000;
inline constexpr std::uint32_t kRealmListReplyDataUnknown5 = 0x00000000;
inline constexpr std::uint32_t kRealmListReplyDataUnknown6 = 0x00000000;
inline constexpr std::uint32_t kRealmListReplyDataUnknown7 = 0x00018210;
inline constexpr std::uint32_t kRealmListReplyDataUnknown8 = 0xffffffff;
inline constexpr std::uint32_t kRealmListReplyDataUnknown9 = 0x00000000;

// ---- RealmListReply 110 payload magic constants ------------------------
inline constexpr std::uint32_t kRealmListReply110Unknown1     = 0x00000000;
inline constexpr std::uint32_t kRealmListReply110DataUnknown1 = 0x00000001;

} // namespace pvpgn::protocol::bnet::realm
