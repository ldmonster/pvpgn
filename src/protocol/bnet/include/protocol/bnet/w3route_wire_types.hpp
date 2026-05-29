// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file w3route_wire_types.hpp
/// Warcraft III anongame-routing wire constants, mirrored from
/// `src/common/bnet_protocol.h` (lines ~73-396). Constants-only;
/// per-message structs deferred until codec impl lands.

#include <cstdint>

namespace pvpgn::protocol::bnet::w3route {

// ---- 16-bit packet type codes (w3route protocol class, low byte 0xf7) ----
inline constexpr std::uint16_t kClientReq            = 0x1ef7;
inline constexpr std::uint16_t kClientLoadingDone    = 0x23f7;
inline constexpr std::uint16_t kServerReady          = 0x14f7;
inline constexpr std::uint16_t kClientAbort          = 0x21f7;
inline constexpr std::uint16_t kServerLoadingAck     = 0x08f7;
inline constexpr std::uint16_t kClientConnected      = 0x3bf7;
inline constexpr std::uint16_t kServerEchoReq        = 0x01f7;
inline constexpr std::uint16_t kClientEchoReply      = 0x46f7;
inline constexpr std::uint16_t kClientGameResult     = 0x2ef7;
inline constexpr std::uint16_t kClientGameResultW3xp = 0x3af7;
inline constexpr std::uint16_t kServerAck            = 0x04f7;
inline constexpr std::uint16_t kServerPlayerInfo     = 0x06f7;
inline constexpr std::uint16_t kServerLevelInfo      = 0x47f7;
inline constexpr std::uint16_t kServerStartGame1     = 0x0af7;
inline constexpr std::uint16_t kServerStartGame2     = 0x0bf7;

// ---- Per-player game-result code (bn_int) ----
inline constexpr std::uint32_t kGameResultLoss = 0x00000003;
inline constexpr std::uint32_t kGameResultWin  = 0x00000004;

// ---- Magic constant inside SERVER_W3ROUTE_ACK ----
inline constexpr std::uint32_t kServerAckUnknown3 = 0x484e2637;

} // namespace pvpgn::protocol::bnet::w3route
