// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure codec for the D2CS ↔ D2GS internal bridge protocol.
///
/// Header (8 bytes, all LE):
///   u16 size    ─┐ total packet length, header included
///   u16 type    ─┤
///   u32 seqno   ─┘ correlation id set by the sender
///
/// Subset implemented now (matches `src/common/d2cs_d2gs_protocol.h`):
///   * 0x12  SETGSINFO    — maxgame + gameflag (both directions, same wire)
///   * 0x13  ECHOREQ      — D2CS → D2GS, empty body
///   * 0x13  ECHOREPLY    — D2GS → D2CS, empty body (same code, direction)
///   * 0x14  CONTROL      — cmd + value (restart/shutdown)
///
/// AUTHREQ/AUTHREPLY (0x10/0x11) and the bulk-state messages are
/// deferred — they shared the same wire code across directions, which
/// the Phase-5 router will tag explicitly per side.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::d2gs {

inline constexpr std::uint16_t kAuthReq   = 0x10;
inline constexpr std::uint16_t kAuthReply = 0x11;
inline constexpr std::uint16_t kSetGsInfo = 0x12;
inline constexpr std::uint16_t kEcho      = 0x13;
inline constexpr std::uint16_t kControl   = 0x14;

inline constexpr std::uint32_t kControlRestart  = 0x01;
inline constexpr std::uint32_t kControlShutdown = 0x02;

struct D2gsHeader {
    std::uint16_t size  = 0;
    std::uint16_t type  = 0;
    std::uint32_t seqno = 0;
    static constexpr std::size_t kSize = 8;
    bool operator==(const D2gsHeader&) const = default;
};

struct SetGsInfo {
    std::uint32_t seqno    = 0;
    std::uint32_t max_game = 0;
    std::uint32_t gameflag = 0;
    bool operator==(const SetGsInfo&) const = default;
};

struct EchoReq {
    std::uint32_t seqno = 0;
    bool operator==(const EchoReq&) const = default;
};

struct EchoReply {
    std::uint32_t seqno = 0;
    bool operator==(const EchoReply&) const = default;
};

struct Control {
    std::uint32_t seqno = 0;
    std::uint32_t cmd   = 0;
    std::uint32_t value = 0;
    bool operator==(const Control&) const = default;
};

/// 0x10 D2CS → D2GS: server probes the game-server identity. Wire is
/// `u32 session_num` + `u32 signlen` + cstring realm name + raw key
/// checksum bytes (length `signlen`, transparently captured here).
struct DownAuthReq {
    std::uint32_t seqno       = 0;
    std::uint32_t session_num = 0;
    std::string   realm_name;
    std::vector<std::byte> key_checksum;
    bool operator==(const DownAuthReq&) const = default;
};

/// 0x11 D2GS → D2CS: game server's signed identity reply. Wire is
/// `u32 version` + `u32 checksum` + `u32 randnum` + `u32 signlen` +
/// 128 sign bytes (fixed buffer, signlen indicates the populated prefix).
struct UpAuthReply {
    std::uint32_t seqno    = 0;
    std::uint32_t version  = 0;
    std::uint32_t checksum = 0;
    std::uint32_t randnum  = 0;
    std::uint32_t signlen  = 0;
    std::array<std::byte, 128> sign{};
    bool operator==(const UpAuthReply&) const = default;
};

inline constexpr std::uint32_t kAuthReplyOk          = 0x00;
inline constexpr std::uint32_t kAuthReplyBadVersion  = 0x01;
inline constexpr std::uint32_t kAuthReplyBadChecksum = 0x02;

/// 0x11 D2CS → D2GS: realm server's verdict on the prior reply. Single
/// `u32 reply` body. Same wire code as UpAuthReply — direction-tagged.
struct DownAuthReply {
    std::uint32_t seqno = 0;
    std::uint32_t reply = kAuthReplyOk;
    bool operator==(const DownAuthReply&) const = default;
};

/// D2CS → D2GS direction.
using DownMessage = std::variant<DownAuthReq, DownAuthReply, SetGsInfo,
                                  EchoReq, Control>;
/// D2GS → D2CS direction.
using UpMessage   = std::variant<UpAuthReply, SetGsInfo, EchoReply>;

core::Result<D2gsHeader>  parse_header(core::ByteView buf);
core::Result<DownMessage> decode_d2cs_to_d2gs(core::ByteView buf);
core::Result<UpMessage>   decode_d2gs_to_d2cs(core::ByteView buf);

core::Status<> encode(Writer& w, const SetGsInfo&     m);
core::Status<> encode(Writer& w, const EchoReq&       m);
core::Status<> encode(Writer& w, const EchoReply&     m);
core::Status<> encode(Writer& w, const Control&       m);
core::Status<> encode(Writer& w, const DownAuthReq&   m);
core::Status<> encode(Writer& w, const DownAuthReply& m);
core::Status<> encode(Writer& w, const UpAuthReply&   m);

}  // namespace pvpgn::protocol::d2gs
