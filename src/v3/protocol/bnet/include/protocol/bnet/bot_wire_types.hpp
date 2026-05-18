// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bot_wire_types.hpp
/// Bot interface event IDs, mirrored from `src/common/bot_protocol.h`.
///
/// The bot protocol is a line-oriented ASCII protocol with no wire
/// structs; the only thing the legacy header defined was a set of
/// `EID_*` constants that name event categories.

#include <cstdint>

namespace pvpgn::protocol::bnet::bot {

inline constexpr std::uint32_t kEidShowUser            = 1001;
inline constexpr std::uint32_t kEidJoin                = 1002;
inline constexpr std::uint32_t kEidLeave               = 1003;
inline constexpr std::uint32_t kEidWhisper             = 1004;
inline constexpr std::uint32_t kEidTalk                = 1005;
inline constexpr std::uint32_t kEidBroadcast           = 1006;
inline constexpr std::uint32_t kEidChannel             = 1007;
inline constexpr std::uint32_t kEidUserFlags           = 1009;
inline constexpr std::uint32_t kEidWhisperSent         = 1010;
inline constexpr std::uint32_t kEidChannelFull         = 1013;
inline constexpr std::uint32_t kEidChannelDoesNotExist = 1014;
inline constexpr std::uint32_t kEidChannelRestricted   = 1015;

}  // namespace pvpgn::protocol::bnet::bot
