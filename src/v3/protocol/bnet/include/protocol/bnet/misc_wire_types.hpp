// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file misc_wire_types.hpp
/// Miscellaneous bnet wire constants: ad banner, echo/ping keepalive,
/// file-info, MOTD (W3 + classic), read-memory, message-box, crash-dump,
/// extra-work / required-work, change-client, UNKNOWN_24. Mirrored from
/// `src/common/bnet_protocol.h`. Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::misc {

// ---- Ad-banner packet codes -------------------------------------------
inline constexpr std::uint16_t kClientAdReq        = 0x15ff;
inline constexpr std::uint16_t kServerAdReply      = 0x15ff;
inline constexpr std::uint16_t kClientAdAck        = 0x21ff;
inline constexpr std::uint16_t kClientAdClick      = 0x16ff;
inline constexpr std::uint16_t kClientAdClick2     = 0x41ff;
inline constexpr std::uint16_t kServerAdClickReply2 = 0x41ff;

// ---- Echo / Ping keepalives -------------------------------------------
inline constexpr std::uint16_t kClientEchoReply = 0x25ff;
inline constexpr std::uint16_t kServerEchoReq   = 0x25ff;
inline constexpr std::uint16_t kClientPingReq   = 0x00ff;
inline constexpr std::uint16_t kServerPingReply = 0x00ff;

// ---- File-info ---------------------------------------------------------
inline constexpr std::uint16_t kClientFileInfoReq   = 0x33ff;
inline constexpr std::uint16_t kServerFileInfoReply = 0x33ff;

inline constexpr std::uint32_t kFileInfoReqTypeTos        = 0x0000001a;
inline constexpr std::uint32_t kFileInfoReqTypeGateways   = 0x0000001b;
inline constexpr std::uint32_t kFileInfoReqTypeGatewaysD2 = 0x80000004;
inline constexpr std::uint32_t kFileInfoReqTypeIcons      = 0x0000001d;
inline constexpr std::uint32_t kFileInfoReqUnknown2       = 0x00000000;

inline constexpr std::uint32_t kFileInfoReplyTypeTosFile    = 0x0000001a;
inline constexpr std::uint32_t kFileInfoReplyTypeGateways   = 0x0000001b;
inline constexpr std::uint32_t kFileInfoReplyTypeGatewaysD2 = 0x80000004;
inline constexpr std::uint32_t kFileInfoReplyTypeExtraWork  = 0x80000005;
inline constexpr std::uint32_t kFileInfoReplyTypeIcons      = 0x0000001d;
inline constexpr std::uint32_t kFileInfoReplyUnknown2       = 0x00000000;

// ---- W3 MOTD ----------------------------------------------------------
inline constexpr std::uint16_t kClientMotdW3 = 0x46ff;
inline constexpr std::uint16_t kServerMotdW3 = 0x46ff;
inline constexpr std::uint8_t  kServerMotdW3MsgType = 0x01;
inline constexpr std::uint32_t kServerMotdW3Welcome = 0x00000000;

// ---- Classic MOTD (CLIENT_MOTDREQ alias same code as CLIENT_MOTD_W3) --
inline constexpr std::uint16_t kClientMotdReq = 0x46ff;

// ---- Read-memory ------------------------------------------------------
inline constexpr std::uint16_t kClientReadMemory = 0x17ff;
inline constexpr std::uint16_t kServerReadMemory = 0x17ff;

// ---- UNKNOWN_24 -------------------------------------------------------
inline constexpr std::uint16_t kClientUnknown24 = 0x24ff;

// ---- Message-box ------------------------------------------------------
inline constexpr std::uint16_t kServerMessageBox = 0x19ff;
inline constexpr std::uint32_t kMessageBoxOk       = 0x00000000;
inline constexpr std::uint32_t kMessageBoxOkCancel = 0x00000001;
inline constexpr std::uint32_t kMessageBoxYesNo    = 0x00000004;

// ---- Required-work / Extra-work --------------------------------------
inline constexpr std::uint16_t kServerRequiredWork = 0x4Cff;
inline constexpr std::uint16_t kClientExtraWork    = 0x4bff;

// ---- Change-client / Crash-dump ---------------------------------------
inline constexpr std::uint16_t kClientChangeClient = 0x5cff;
inline constexpr std::uint16_t kClientCrashDump    = 0x5dff;

} // namespace pvpgn::protocol::bnet::misc
