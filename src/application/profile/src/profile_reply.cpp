// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/profile/profile_reply.hpp"

#include <cstdint>

namespace pvpgn::application::profile {

namespace pb = pvpgn::protocol::bnet;

namespace {

inline void put_u8(std::vector<std::uint8_t>& v, std::uint8_t x) {
    v.push_back(x);
}

inline void put_le16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
}

inline void put_le32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xFF));
}

// Append one ladder section: tag(LE u32) + wins(LE u16) + losses(LE u16)
// + level(u8) + calc(u8) + xp(LE u16) + rank(LE u32) = 16 bytes.
//
// Mirrors the legacy:
//   bn_int_set(temp, 0x534F4C4F /* SOLO etc. */); append 4
//   bn_int_set(temp, wins);                       append 2
//   ...
// All multi-byte fields are LE because legacy `bn_int_set` is LE.
void append_ladder_section(
    std::vector<std::uint8_t>& out,
    std::uint32_t              tag,
    const LadderStats&         s) {
    put_le32(out, tag);
    put_le16(out, s.wins);
    put_le16(out, s.losses);
    put_u8 (out, s.level);
    put_u8 (out, s.calc);
    put_le16(out, s.xp);
    put_le32(out, s.rank);
}

}  // namespace

pb::AnonGameProfileReply build_profile_reply(const ProfileInputs& in) {
    pb::AnonGameProfileReply r{};
    r.count = in.count;
    r.icon  = in.profile_icon;

    // No-stats path: emit rescount=0 + 2 trailing zero bytes (legacy
    // `_client_anongame_profile` stub branch).
    if (!in.has_stats) {
        r.rescount = 0;
        r.data.assign(2, std::uint8_t{0});
        return r;
    }

    // Full payload path. rescount counts how many ladder sections we
    // emit (capped at 3: solo/team/ffa).
    std::uint8_t rescount = 0;

    // Tags are the four-byte ASCII strings written little-endian, i.e.
    // "SOLO", "TEAM", "FFA " on the wire.
    if (in.solo.level > 0) {
        append_ladder_section(r.data, 0x534F4C4Fu /* SOLO */, in.solo);
        ++rescount;
    }
    if (in.team.level > 0) {
        append_ladder_section(r.data, 0x5445414Du /* TEAM */, in.team);
        ++rescount;
    }
    if (in.ffa.level > 0) {
        append_ladder_section(r.data, 0x46464120u /* FFA  */, in.ffa);
        ++rescount;
    }
    r.rescount = rescount;

    // Race stats header (legacy literal 0x06).
    put_u8(r.data, 0x06);
    put_le16(r.data, in.random.wins);
    put_le16(r.data, in.random.losses);
    put_le16(r.data, in.humans.wins);
    put_le16(r.data, in.humans.losses);
    put_le16(r.data, in.orcs.wins);
    put_le16(r.data, in.orcs.losses);
    put_le16(r.data, in.undead.wins);
    put_le16(r.data, in.undead.losses);
    put_le16(r.data, in.nightelves.wins);
    put_le16(r.data, in.nightelves.losses);
    put_le16(r.data, in.demons.wins);
    put_le16(r.data, in.demons.losses);

    // AT team count (1 byte) + per-team blocks. Cap at 16 to match
    // the legacy `if ((teamcount >= 16)) break` guard.
    const std::uint8_t teamcount = static_cast<std::uint8_t>(
        in.teams.size() > 16 ? 16 : in.teams.size());
    put_u8(r.data, teamcount);
    for (std::uint8_t ti = 0; ti < teamcount; ++ti) {
        const auto& t = in.teams[ti];
        put_le32(r.data, t.team_tag);
        put_le16(r.data, t.wins);
        put_le16(r.data, t.losses);
        put_u8 (r.data, t.level);
        put_u8 (r.data, t.calc);
        put_le16(r.data, t.xp);
        put_le32(r.data, t.rank);
        // Raw 8-byte bnettime; legacy emits via bnettime_to_bn_long
        // already in LE, so we copy verbatim.
        for (auto b : t.lastgame_bn_long) put_u8(r.data, b);
        put_u8(r.data, t.size_minus_one);
        // Other members' names (NUL-terminated, in legacy order).
        for (const auto& name : t.other_members) {
            for (char c : name) put_u8(r.data, static_cast<std::uint8_t>(c));
            put_u8(r.data, 0x00);
        }
    }

    return r;
}

}  // namespace pvpgn::application::profile
