// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/bnet/codec_extended.hpp"

#include "protocol/common/reader.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

// --- SID_CHARLIST (0x37) — D2 character list ---
// Client → Server: product_id (u32)
// Server → Client: num_chars (u32), then for each: char_name (string), char_statstring (blob)

core::Status<> encode(Writer& w, const CharListRequest& msg) {
    w.write_u32(msg.open_count);
    return core::Status<>::Ok();
}

core::Status<> encode(Writer& w, const CharListReply& msg) {
    w.write_u32(msg.unknown1);
    // TODO: Implement full D2 character encoding (names + statstrings)
    // For now, stub that writes the header only
    return core::Status<>::Ok();
}

// --- SID_CLANMEMBERLIST (0x7D) — Clan roster ---
// Client → Server: cookie (u32)
// Server → Client: cookie (u32), num_members (u32), then for each:
//   - name (string)
//   - rank (u8) — 0=peon, 1=grunt, 2=shaman, 3=chieftain
//   - status (u8) — 0=offline, 1=online
//   - location (string) — channel or game name

core::Status<> encode(Writer& w, const ClanMemberListRequest& msg) {
    w.write_u32(msg.cookie);
    return core::Status<>::Ok();
}

core::Status<> encode(Writer& w, const ClanMemberListReply& msg) {
    w.write_u32(msg.cookie);
    // TODO: Implement full member list encoding
    return core::Status<>::Ok();
}

// --- SID_CLANINFO (0x82) — Clan info ---
// Client → Server: cookie (u32)
// Server → Client: cookie (u32), unknown (u8), clan_tag (string), clan_name (string),
//                  clan_rank (u8), fail (u32)
// On fail != 0, the rest of the fields are omitted.

core::Status<> encode(Writer& w, const ClanInfoRequest& msg) {
    w.write_u32(msg.cookie);
    return core::Status<>::Ok();
}

core::Status<> encode(Writer& w, const ClanInfoReply& msg) {
    w.write_u32(msg.cookie);
    w.write_u8(msg.unknown);
    if (msg.fail == 0) {
        w.write_string(msg.clan_tag);
        w.write_string(msg.clan_name);
        w.write_u8(msg.clan_rank);
    }
    w.write_u32(msg.fail);
    return core::Status<>::Ok();
}

}  // namespace pvpgn::protocol::bnet
