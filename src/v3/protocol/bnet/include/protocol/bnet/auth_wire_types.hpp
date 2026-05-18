// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file auth_wire_types.hpp
/// Battle.net auth-handshake wire constants (CompInfo, CompReply,
/// SessionKey, CountryInfo, AuthInfo, AuthReq, ProgIdent, RegSnoop,
/// IconReq), mirrored from `src/common/bnet_protocol.h`
/// (lines ~398-1019). Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::auth {

// ---- Packet type codes -------------------------------------------------
inline constexpr std::uint16_t kClientCompInfo1   = 0x05ff;
inline constexpr std::uint16_t kClientCompInfo2   = 0x1eff;
inline constexpr std::uint16_t kServerCompReply   = 0x05ff;

inline constexpr std::uint16_t kServerSessionKey1 = 0x28ff;
inline constexpr std::uint16_t kServerSessionKey2 = 0x1dff;

inline constexpr std::uint16_t kClientCountryInfo1 = 0x12ff;

inline constexpr std::uint16_t kClientAuthInfo     = 0x50ff;

inline constexpr std::uint16_t kClientProgIdent    = 0x06ff;
inline constexpr std::uint16_t kServerAuthReq1     = 0x06ff;
inline constexpr std::uint16_t kServerAuthReq109   = 0x50ff;

inline constexpr std::uint16_t kClientAuthReq1     = 0x07ff;
inline constexpr std::uint16_t kServerAuthReply1   = 0x07ff;

inline constexpr std::uint16_t kServerAuthReply109 = 0x51ff;
inline constexpr std::uint16_t kClientAuthReq109   = 0x51ff;

inline constexpr std::uint16_t kServerRegSnoopReq  = 0x18ff;
inline constexpr std::uint16_t kClientRegSnoopReply = 0x18ff;

inline constexpr std::uint16_t kClientIconReq      = 0x2dff;
inline constexpr std::uint16_t kServerIconReply    = 0x2dff;

// ---- CompInfo / CompReply magic registration fields --------------------
inline constexpr std::uint32_t kCompRegVersion  = 0x00000001;
inline constexpr std::uint32_t kCompRegAuth     = 0xaa8843d1;
inline constexpr std::uint32_t kCompClientId    = 0x001b9dda;
inline constexpr std::uint32_t kCompClientToken = 0xab69f79a;

// ---- SessionKey ------------------------------------------------------
inline constexpr std::uint32_t kServerSessionKey2Unknown1 = 0x00004df3;

// ---- AuthReq_109 logontype --------------------------------------------
inline constexpr std::uint32_t kServerAuthReq109Logontype     = 0x0000000;
inline constexpr std::uint32_t kServerAuthReq109LogontypeW3   = 0x00000002;
inline constexpr std::uint32_t kServerAuthReq109LogontypeW3xp = 0x00000002;

// ---- AuthReply1 result codes ------------------------------------------
inline constexpr std::uint32_t kServerAuthReply1MessageBadVersion = 0x00000000;
inline constexpr std::uint32_t kServerAuthReply1MessageUpdate     = 0x00000001;
inline constexpr std::uint32_t kServerAuthReply1MessageOk         = 0x00000002;

// ---- AuthReply_109 result codes ---------------------------------------
inline constexpr std::uint32_t kServerAuthReply109MessageOk         = 0x00000000;
inline constexpr std::uint32_t kServerAuthReply109MessageUpdate     = 0x00000100;
inline constexpr std::uint32_t kServerAuthReply109MessageBadVersion = 0x00000101;

// ---- RegSnoop predefined HKEYs ----------------------------------------
inline constexpr std::uint32_t kRegSnoopHkeyClassesRoot       = 0x80000000;
inline constexpr std::uint32_t kRegSnoopHkeyCurrentUser       = 0x80000001;
inline constexpr std::uint32_t kRegSnoopHkeyLocalMachine      = 0x80000002;
inline constexpr std::uint32_t kRegSnoopHkeyUsers             = 0x80000003;
inline constexpr std::uint32_t kRegSnoopHkeyPerformanceData   = 0x80000004;
inline constexpr std::uint32_t kRegSnoopHkeyCurrentConfig     = 0x80000005;
inline constexpr std::uint32_t kRegSnoopHkeyDynData           = 0x80000006;
inline constexpr std::uint32_t kRegSnoopHkeyPerformanceText   = 0x80000050;
inline constexpr std::uint32_t kRegSnoopHkeyPerformanceNlsText = 0x80000060;

} // namespace pvpgn::protocol::bnet::auth
