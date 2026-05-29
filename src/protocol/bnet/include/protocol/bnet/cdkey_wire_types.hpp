// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file cdkey_wire_types.hpp
/// CDKEY / CDKEY2 / CDKEY3 + reply wire constants, mirrored from
/// `src/common/bnet_protocol.h` (lines ~1057-1196, 1879-1898).
/// Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::cdkey {

// ---- Packet type codes -------------------------------------------------
inline constexpr std::uint16_t kClientCdkey       = 0x30ff;
inline constexpr std::uint16_t kServerCdkeyReply  = 0x30ff;

inline constexpr std::uint16_t kClientCdkey2      = 0x36ff;
inline constexpr std::uint16_t kServerCdkeyReply2 = 0x36ff;

inline constexpr std::uint16_t kClientCdkey3      = 0x42ff;
inline constexpr std::uint16_t kServerCdkeyReply3 = 0x42ff;

// ---- CdkeyReply / CdkeyReply2 message codes (same set) ----------------
inline constexpr std::uint32_t kCdkeyReplyMessageOk       = 0x00000001;
inline constexpr std::uint32_t kCdkeyReplyMessageBad      = 0x00000002;
inline constexpr std::uint32_t kCdkeyReplyMessageWrongApp = 0x00000003;
inline constexpr std::uint32_t kCdkeyReplyMessageError    = 0x00000004;
inline constexpr std::uint32_t kCdkeyReplyMessageInUse    = 0x00000005;

// ---- CdkeyReply3 message codes ----------------------------------------
inline constexpr std::uint32_t kCdkeyReply3MessageOk = 0x00000000;

// ---- Cdkey2 spawn flags -----------------------------------------------
inline constexpr std::uint32_t kCdkey2SpawnTrue  = 0x00000001;
inline constexpr std::uint32_t kCdkey2SpawnFalse = 0x00000000;

// ---- Cdkey3 magic / unknown constants ---------------------------------
inline constexpr std::uint32_t kCdkey3Unknown1 = 0xffffffff;
inline constexpr std::uint32_t kCdkey3Unknown2 = 0x00000001;
inline constexpr std::uint32_t kCdkey3Unknown3 = 0x00000000;
inline constexpr std::uint32_t kCdkey3Unknown4 = 0x00000010;
inline constexpr std::uint32_t kCdkey3Unknown5 = 0x00000006;
inline constexpr std::uint32_t kCdkey3Unknown6 = 0x00123456;
inline constexpr std::uint32_t kCdkey3Unknown7 = 0x00000000;

} // namespace pvpgn::protocol::bnet::cdkey
