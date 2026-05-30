// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

core::Result<ClanInfoRequest> decode_claninfo_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanInfoRequest m;
    RD_U32(m.cookie);
    RD_U32(m.clan_tag);
    RD_STR(m.player_name);
    return m;
}

core::Result<ClanInfoReply> decode_claninfo_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanInfoReply m;
    RD_U32(m.cookie);
    auto f = r.read_le<std::uint8_t>();
    if (!f) return core::fail(f.error());
    m.fail = f.value();
    if (m.fail == 0 && !r.empty()) {
        RD_STR(m.clan_name);
        auto rk = r.read_le<std::uint8_t>();
        if (!rk) return core::fail(rk.error());
        m.rank = rk.value();
        RD_U32(m.join_time);
    }
    return m;
}

// --- SID_READUSERDATA (0x26) / SID_WRITEUSERDATA (0x27) -------------------

// Read both vectors as `count` cstrings; `count` was already decoded.

core::Result<ClanCreateRequest> decode_clan_create_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanCreateRequest m;
    RD_U32(m.cookie);
    RD_U32(m.clan_tag);
    return m;
}

core::Result<ClanCreateReply> decode_clan_create_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanCreateReply m;
    RD_U32(m.cookie);
    auto cr = r.read_le<std::uint8_t>();
    if (!cr) return core::fail(cr.error());
    m.check_result = cr.value();
    auto fc = r.read_le<std::uint8_t>();
    if (!fc) return core::fail(fc.error());
    constexpr std::uint8_t kFriendLimit = 64u;
    if (fc.value() > kFriendLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: clan create reply friend_count exceeds limit"});
    }
    m.friend_names.reserve(fc.value());
    for (std::uint8_t i = 0; i < fc.value(); ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        m.friend_names.emplace_back(s.value());
    }
    return m;
}

core::Result<ClanDisbandRequest> decode_clan_disband_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanDisbandRequest m;
    RD_U32(m.cookie);
    return m;
}

core::Result<ClanNewChiefRequest> decode_clan_newchief_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanNewChiefRequest m;
    RD_U32(m.cookie);
    RD_STR(m.player_name);
    return m;
}

core::Result<ClanInviteRequest> decode_clan_invite_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanInviteRequest m;
    RD_U32(m.cookie);
    RD_STR(m.player_name);
    return m;
}

core::Result<ClanMemberRemoveRequest> decode_clan_member_remove_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMemberRemoveRequest m;
    RD_U32(m.cookie);
    RD_STR(m.player_name);
    return m;
}

core::Result<ClanMemberRankUpdateRequest>
decode_clan_member_rank_update_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMemberRankUpdateRequest m;
    RD_U32(m.cookie);
    RD_STR(m.player_name);
    auto rk = r.read_le<std::uint8_t>();
    if (!rk) return core::fail(rk.error());
    m.new_rank = rk.value();
    return m;
}

// Shared decoder for the cookie+result-byte reply family (0x73, 0x74, 0x77,
// 0x78, 0x7A). The caller passes the SID code so the variant knows which
// command this is the reply to.

core::Result<ClanGenericResultReply> decode_clan_generic_result_reply(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanGenericResultReply m;
    m.sid = pkt.header.code;
    RD_U32(m.cookie);
    auto rs = r.read_le<std::uint8_t>();
    if (!rs) return core::fail(rs.error());
    m.result = rs.value();
    return m;
}

core::Result<ClanMotdChange> decode_clan_motd_change(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMotdChange m;
    RD_U32(m.unknown1);
    RD_STR(m.motd);
    return m;
}

core::Result<ClanMotdRequest> decode_clan_motd_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMotdRequest m;
    RD_U32(m.cookie);
    return m;
}

core::Result<ClanMotdReply> decode_clan_motd_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMotdReply m;
    RD_U32(m.cookie);
    RD_U32(m.unknown1);
    RD_STR(m.motd);
    return m;
}

// --- 0x71 / 0x72 / 0x79 multi-cookie invite chains -------------------------

constexpr std::uint8_t kClanInviteFriendLimit = 64u;

core::Status<> read_friend_list(Reader& r, std::vector<std::string>& out) {
    auto fc = r.read_le<std::uint8_t>();
    if (!fc) return core::fail(fc.error());
    if (fc.value() > kClanInviteFriendLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: clan invite friend_count exceeds limit"});
    }
    out.reserve(fc.value());
    for (std::uint8_t i = 0; i < fc.value(); ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        out.emplace_back(s.value());
    }
    return core::ok();
}

core::Result<ClanCreateInviteRequest> decode_clan_create_invite_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanCreateInviteRequest m;
    RD_U32(m.cookie);
    RD_STR(m.clan_name);
    RD_U32(m.clan_tag);
    auto s = read_friend_list(r, m.friend_names);
    if (!s) return core::fail(s.error());
    return m;
}

core::Result<ClanCreateInviteSummary> decode_clan_create_invite_summary(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanCreateInviteSummary m;
    RD_U32(m.cookie);
    auto st = r.read_le<std::uint8_t>();
    if (!st) return core::fail(st.error());
    m.status = st.value();
    // failed_member is only present when status != 0.
    if (m.status != 0 && !r.empty()) RD_STR(m.failed_member);
    return m;
}

core::Result<ClanCreateInviteForward> decode_clan_create_invite_forward(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanCreateInviteForward m;
    RD_U32(m.cookie);
    RD_U32(m.clan_tag);
    RD_STR(m.clan_name);
    RD_STR(m.clan_creator);
    auto s = read_friend_list(r, m.friend_names);
    if (!s) return core::fail(s.error());
    return m;
}

core::Result<ClanCreateInviteResponse> decode_clan_create_invite_response(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanCreateInviteResponse m;
    RD_U32(m.cookie);
    RD_U32(m.clan_tag);
    RD_STR(m.clan_creator);
    auto rp = r.read_le<std::uint8_t>();
    if (!rp) return core::fail(rp.error());
    m.reply = rp.value();
    return m;
}

core::Result<ClanInvite2Forward> decode_clan_invite2_forward(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanInvite2Forward m;
    RD_U32(m.cookie);
    RD_U32(m.clan_tag);
    RD_STR(m.clan_name);
    RD_STR(m.inviter_name);
    return m;
}

core::Result<ClanInvite2Response> decode_clan_invite2_response(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanInvite2Response m;
    RD_U32(m.cookie);
    RD_U32(m.clan_tag);
    RD_STR(m.inviter_name);
    auto rp = r.read_le<std::uint8_t>();
    if (!rp) return core::fail(rp.error());
    m.reply = rp.value();
    return m;
}

// --- 0x7D / 0x7E / 0x7F clan member-list and event notifies ----------------

constexpr std::uint8_t kClanMemberListLimit = 200u;

core::Result<ClanMemberListRequest> decode_clan_memberlist_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMemberListRequest m;
    RD_U32(m.cookie);
    return m;
}

core::Result<ClanMemberListReply> decode_clan_memberlist_reply(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMemberListReply m;
    RD_U32(m.cookie);
    auto mc = r.read_le<std::uint8_t>();
    if (!mc) return core::fail(mc.error());
    if (mc.value() > kClanMemberListLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: clan member_count exceeds limit"});
    }
    m.members.reserve(mc.value());
    for (std::uint8_t i = 0; i < mc.value(); ++i) {
        ClanMemberEntry e;
        auto name = r.read_cstring();
        if (!name) return core::fail(name.error());
        e.name = name.value();
        auto rank = r.read_le<std::uint8_t>();
        if (!rank) return core::fail(rank.error());
        e.rank = rank.value();
        auto online = r.read_le<std::uint8_t>();
        if (!online) return core::fail(online.error());
        e.online_status = online.value();
        auto loc = r.read_cstring();
        if (!loc) return core::fail(loc.error());
        e.location = loc.value();
        m.members.emplace_back(std::move(e));
    }
    return m;
}

core::Result<ClanMemberRemovedNotify> decode_clan_member_removed_notify(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMemberRemovedNotify m;
    RD_STR(m.name);
    return m;
}

core::Result<ClanMemberUpdate> decode_clan_member_update(const Packet& pkt) {
    Reader r{pkt.payload};
    ClanMemberUpdate m;
    RD_STR(m.name);
    auto rank = r.read_le<std::uint8_t>();
    if (!rank) return core::fail(rank.error());
    m.rank = rank.value();
    auto online = r.read_le<std::uint8_t>();
    if (!online) return core::fail(online.error());
    m.online_status = online.value();
    RD_STR(m.location);
    return m;
}

// =========================================================================
// Legacy / OLS / pre-NLS decoders (added by the "implement all SIDs" pass).
// =========================================================================

#define RD_U64(target) \
    do {                                                                  \
        auto _v = r.read_le<std::uint64_t>();                             \
        if (!_v) return core::fail(_v.error());                           \
        (target) = _v.value();                                            \
    } while (0)
#define RD_U16(target) \
    do {                                                                  \
        auto _v = r.read_le<std::uint16_t>();                             \
        if (!_v) return core::fail(_v.error());                           \
        (target) = _v.value();                                            \
    } while (0)
#define RD_HASH5(arr) \
    do {                                                                  \
        for (auto& _x : (arr)) { RD_U32(_x); }                            \
    } while (0)

// 0x05 CLIENT_COMPINFO1


} // namespace detail

core::Status<> encode(Writer& w, const ClanInfoRequest& m) {
    w.begin_bnet_packet(kSidClanInfo);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint32_t>(m.clan_tag);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanInfoReply& m) {
    w.begin_bnet_packet(kSidClanInfo);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint8_t>(m.fail);
    if (m.fail == 0) {
        w.write_cstring(m.clan_name);
        w.write_le<std::uint8_t>(m.rank);
        w.write_le<std::uint32_t>(m.join_time);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanCreateRequest& m) {
    w.begin_bnet_packet(kSidClanCreate);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint32_t>(m.clan_tag);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanCreateReply& m) {
    w.begin_bnet_packet(kSidClanCreate);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint8_t>(m.check_result);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.friend_names.size()));
    for (const auto& s : m.friend_names) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanDisbandRequest& m) {
    w.begin_bnet_packet(kSidClanDisband);
    w.write_le<std::uint32_t>(m.cookie);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanNewChiefRequest& m) {
    w.begin_bnet_packet(kSidClanMemberNewChief);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanInviteRequest& m) {
    w.begin_bnet_packet(kSidClanInvite);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMemberRemoveRequest& m) {
    w.begin_bnet_packet(kSidClanMemberRemove);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMemberRankUpdateRequest& m) {
    w.begin_bnet_packet(kSidClanMemberRankUpdate);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_cstring(m.player_name);
    w.write_le<std::uint8_t>(m.new_rank);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanGenericResultReply& m) {
    // Caller-supplied SID picks which packet this reply belongs to. Empty
    // / zero SID falls back to CLAN_DISBAND (the most common shape).
    const std::uint8_t sid = m.sid != 0 ? m.sid : kSidClanDisband;
    w.begin_bnet_packet(sid);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint8_t>(m.result);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMotdChange& m) {
    w.begin_bnet_packet(kSidClanMotdChange);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_cstring(m.motd);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMotdRequest& m) {
    w.begin_bnet_packet(kSidClanMotd);
    w.write_le<std::uint32_t>(m.cookie);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMotdReply& m) {
    w.begin_bnet_packet(kSidClanMotd);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_cstring(m.motd);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanCreateInviteRequest& m) {
    w.begin_bnet_packet(kSidClanCreateInvite);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_cstring(m.clan_name);
    w.write_le<std::uint32_t>(m.clan_tag);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.friend_names.size()));
    for (const auto& s : m.friend_names) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanCreateInviteSummary& m) {
    w.begin_bnet_packet(kSidClanCreateInvite);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint8_t>(m.status);
    if (m.status != 0) w.write_cstring(m.failed_member);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanCreateInviteForward& m) {
    w.begin_bnet_packet(kSidClanCreateInvite2);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint32_t>(m.clan_tag);
    w.write_cstring(m.clan_name);
    w.write_cstring(m.clan_creator);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.friend_names.size()));
    for (const auto& s : m.friend_names) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanCreateInviteResponse& m) {
    w.begin_bnet_packet(kSidClanCreateInvite2);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint32_t>(m.clan_tag);
    w.write_cstring(m.clan_creator);
    w.write_le<std::uint8_t>(m.reply);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanInvite2Forward& m) {
    w.begin_bnet_packet(kSidClanInvite2);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint32_t>(m.clan_tag);
    w.write_cstring(m.clan_name);
    w.write_cstring(m.inviter_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanInvite2Response& m) {
    w.begin_bnet_packet(kSidClanInvite2);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint32_t>(m.clan_tag);
    w.write_cstring(m.inviter_name);
    w.write_le<std::uint8_t>(m.reply);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMemberListRequest& m) {
    w.begin_bnet_packet(kSidClanMemberList);
    w.write_le<std::uint32_t>(m.cookie);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMemberListReply& m) {
    w.begin_bnet_packet(kSidClanMemberList);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.members.size()));
    for (const auto& e : m.members) {
        w.write_cstring(e.name);
        w.write_le<std::uint8_t>(e.rank);
        w.write_le<std::uint8_t>(e.online_status);
        w.write_cstring(e.location);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMemberRemovedNotify& m) {
    w.begin_bnet_packet(kSidClanMemberRemoved);
    w.write_cstring(m.name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ClanMemberUpdate& m) {
    w.begin_bnet_packet(kSidClanMemberUpdate);
    w.write_cstring(m.name);
    w.write_le<std::uint8_t>(m.rank);
    w.write_le<std::uint8_t>(m.online_status);
    w.write_cstring(m.location);
    return w.finalize_bnet_packet();
}


} // namespace pvpgn::protocol::bnet
