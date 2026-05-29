// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec_extended.hpp
/// Extended BNet SID codecs for less common messages.
///
/// This file contains encode/decode functions for:
/// - SID_GETCHARLIST (0x37) / SID_CHARLIST (D2 character list)
/// - SID_CHARCREATE (0x24) (not yet in main codec)
/// - SID_CLANMEMBERLIST (0x7D) — clan roster
/// - SID_CLANINFO (0x82) — clan info response
///
/// These follow the same encode/decode pattern as codec.hpp.

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

// --- Extended encoders for legacy/specialized SIDs ---

/// SID_CHARLIST (0x37) — D2 legacy character list encoding.
core::Status<> encode(Writer& w, const CharListRequest&);
core::Status<> encode(Writer& w, const CharListReply&);

/// SID_CLANMEMBERLIST (0x7D) — Clan member list request/reply.
core::Status<> encode(Writer& w, const ClanMemberListRequest&);
core::Status<> encode(Writer& w, const ClanMemberListReply&);

/// SID_CLANINFO (0x82) — Clan information request/reply.
core::Status<> encode(Writer& w, const ClanInfoRequest&);
core::Status<> encode(Writer& w, const ClanInfoReply&);

}  // namespace pvpgn::protocol::bnet
