// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure codec for the Battle.net SID protocol.
///
/// `decode_client(packet)` / `decode_server(packet)` return a tagged
/// variant of all SIDs we currently understand. Unknown SIDs return
/// `core::Error{Unimplemented, "SID 0xNN"}` so the caller can choose to
/// log+drop or close. Malformed payloads return `OutOfRange` /
/// `InvalidArgument` from the underlying `Reader`.

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

/// Decode one inbound (client → server) SID packet.
core::Result<ClientMessage> decode_client(const Packet& pkt);

/// Decode one inbound (server → client) SID packet (used by replay
/// tests + d2cs/bnpcap-style tools).
core::Result<ServerMessage> decode_server(const Packet& pkt);

// --- encode -------------------------------------------------------------

core::Status<> encode(Writer& w, const Null&);
core::Status<> encode(Writer& w, const Ping&);
core::Status<> encode(Writer& w, const AuthInfo&);
core::Status<> encode(Writer& w, const AuthCheckReply&);
core::Status<> encode(Writer& w, const LogonResponse2&);
core::Status<> encode(Writer& w, const LogonResponse2Reply&);
core::Status<> encode(Writer& w, const JoinChannel&);
core::Status<> encode(Writer& w, const EnterChatRequest&);
core::Status<> encode(Writer& w, const EnterChatReply&);
core::Status<> encode(Writer& w, const ChatCommand&);
core::Status<> encode(Writer& w, const ChatEvent&);

}  // namespace pvpgn::protocol::bnet
