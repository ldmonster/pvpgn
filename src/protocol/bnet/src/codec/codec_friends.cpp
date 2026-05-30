// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

core::Result<FriendsListRequest> decode_friendslist_request(const Packet& pkt) {
    auto s = check_empty_body(pkt);
    if (!s) return core::fail(s.error());
    return FriendsListRequest{};
}

core::Result<FriendsListReply> decode_friendslist_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    FriendsListReply m;
    auto fc = r.read_le<std::uint8_t>();
    if (!fc) return core::fail(fc.error());
    const auto count = fc.value();
    if (count > 200) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "FriendsListReply count exceeds 200"});
    }
    m.entries.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        FriendsListEntry e;
        RD_STR(e.name);
        auto st = r.read_le<std::uint8_t>();
        if (!st) return core::fail(st.error());
        e.status = st.value();
        auto lo = r.read_le<std::uint8_t>();
        if (!lo) return core::fail(lo.error());
        e.location = lo.value();
        RD_U32(e.client_tag);
        RD_STR(e.location_name);
        m.entries.push_back(std::move(e));
    }
    return m;
}

// --- SID_FRIENDINFO (0x66) -------------------------------------------------

core::Result<FriendInfoRequest> decode_friendinfo_request(const Packet& pkt) {
    Reader r{pkt.payload};
    FriendInfoRequest m;
    auto n = r.read_le<std::uint8_t>();
    if (!n) return core::fail(n.error());
    m.friend_num = n.value();
    return m;
}

core::Result<FriendInfoReply> decode_friendinfo_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    FriendInfoReply m;
    auto fn = r.read_le<std::uint8_t>();
    if (!fn) return core::fail(fn.error());
    m.friend_num = fn.value();
    auto ty = r.read_le<std::uint8_t>();
    if (!ty) return core::fail(ty.error());
    m.type = ty.value();
    auto st = r.read_le<std::uint8_t>();
    if (!st) return core::fail(st.error());
    m.status = st.value();
    RD_U32(m.client_tag);
    RD_STR(m.game_name);
    return m;
}

// --- SID_FRIENDADD (0x67) / DEL (0x68) / MOVE (0x69) ----------------------

core::Result<FriendAddAck> decode_friendadd_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    FriendAddAck m;
    RD_STR(m.name);
    auto st = r.read_le<std::uint8_t>();
    if (!st) return core::fail(st.error());
    m.status = st.value();
    auto lo = r.read_le<std::uint8_t>();
    if (!lo) return core::fail(lo.error());
    m.location = lo.value();
    RD_U32(m.client_tag);
    RD_STR(m.location_name);
    return m;
}

core::Result<FriendDelAck> decode_frienddel_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    FriendDelAck m;
    auto n = r.read_le<std::uint8_t>();
    if (!n) return core::fail(n.error());
    m.friend_num = n.value();
    return m;
}

core::Result<FriendMoveAck> decode_friendmove_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    FriendMoveAck m;
    auto p1 = r.read_le<std::uint8_t>();
    if (!p1) return core::fail(p1.error());
    m.pos1 = p1.value();
    auto p2 = r.read_le<std::uint8_t>();
    if (!p2) return core::fail(p2.error());
    m.pos2 = p2.value();
    return m;
}

// --- SID_ARRANGEDTEAM_* (0x60..0x63, 0xFD) ---------------------------------

constexpr std::uint8_t kArrangedTeamFriendLimit = 64u;

core::Result<ArrangedTeamFriendScreenRequest> decode_arrangedteam_friendscreen_request(
    const Packet& /*pkt*/) {
    return ArrangedTeamFriendScreenRequest{};
}

core::Result<ArrangedTeamFriendScreenReply> decode_arrangedteam_friendscreen_reply(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ArrangedTeamFriendScreenReply m;
    auto fc = r.read_le<std::uint8_t>();
    if (!fc) return core::fail(fc.error());
    if (fc.value() > kArrangedTeamFriendLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: arranged-team friend_count exceeds limit"});
    }
    m.names.reserve(fc.value());
    for (std::uint8_t i = 0; i < fc.value(); ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        m.names.emplace_back(s.value());
    }
    return m;
}

core::Result<ArrangedTeamInviteFriendRequest> decode_arrangedteam_invite_friend_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ArrangedTeamInviteFriendRequest m;
    RD_U32(m.count);
    RD_U32(m.id);
    RD_U32(m.unknown1);
    auto nf = r.read_le<std::uint8_t>();
    if (!nf) return core::fail(nf.error());
    if (nf.value() > kArrangedTeamFriendLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: arranged-team invite numfriends exceeds limit"});
    }
    m.friends.reserve(nf.value());
    for (std::uint8_t i = 0; i < nf.value(); ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        m.friends.emplace_back(s.value());
    }
    return m;
}

core::Result<ArrangedTeamInviteFriendAck> decode_arrangedteam_invite_friend_ack(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ArrangedTeamInviteFriendAck m;
    RD_U32(m.count);
    RD_U32(m.id);
    RD_U32(m.timestamp);
    auto ts = r.read_le<std::uint8_t>();
    if (!ts) return core::fail(ts.error());
    m.team_size = ts.value();
    for (std::size_t i = 0; i < m.info.size(); ++i) {
        auto v = r.read_le<std::uint32_t>();
        if (!v) return core::fail(v.error());
        m.info[i] = v.value();
    }
    return m;
}

core::Result<ArrangedTeamMemberDecline> decode_arrangedteam_member_decline(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ArrangedTeamMemberDecline m;
    RD_U32(m.count);
    RD_U32(m.action);
    RD_STR(m.decliner_name);
    return m;
}

core::Result<ArrangedTeamSendInvite> decode_arrangedteam_send_invite(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ArrangedTeamSendInvite m;
    RD_U32(m.count);
    RD_U32(m.id);
    RD_U32(m.inviter_ip);
    auto p = r.read_le<std::uint16_t>();
    if (!p) return core::fail(p.error());
    m.port = p.value();
    auto nf = r.read_le<std::uint8_t>();
    if (!nf) return core::fail(nf.error());
    if (nf.value() > kArrangedTeamFriendLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: arranged-team send-invite numfriends exceeds limit"});
    }
    auto inv = r.read_cstring();
    if (!inv) return core::fail(inv.error());
    m.inviter_name = inv.value();
    m.other_names.reserve(nf.value());
    for (std::uint8_t i = 0; i < nf.value(); ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        m.other_names.emplace_back(s.value());
    }
    return m;
}

core::Result<ArrangedTeamAcceptDeclineInvite> decode_arrangedteam_accept_decline_invite(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ArrangedTeamAcceptDeclineInvite m;
    RD_U32(m.count);
    RD_U32(m.id);
    RD_U32(m.option);
    RD_STR(m.inviter_name);
    return m;
}

core::Result<ArrangedTeamAcceptInvite> decode_arrangedteam_accept_invite(
    const Packet& /*pkt*/) {
    return ArrangedTeamAcceptInvite{};
}

// --- SID_STARTADVEX3 (0x1C) ------------------------------------------------


} // namespace detail

core::Status<> encode(Writer& w, const FriendsListRequest&) {
    w.begin_bnet_packet(kSidFriendsList);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FriendsListReply& m) {
    w.begin_bnet_packet(kSidFriendsList);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.entries.size()));
    for (const auto& e : m.entries) {
        w.write_cstring(e.name);
        w.write_le<std::uint8_t>(e.status);
        w.write_le<std::uint8_t>(e.location);
        w.write_le<std::uint32_t>(e.client_tag);
        w.write_cstring(e.location_name);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FriendInfoRequest& m) {
    w.begin_bnet_packet(kSidFriendInfo);
    w.write_le<std::uint8_t>(m.friend_num);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FriendInfoReply& m) {
    w.begin_bnet_packet(kSidFriendInfo);
    w.write_le<std::uint8_t>(m.friend_num);
    w.write_le<std::uint8_t>(m.type);
    w.write_le<std::uint8_t>(m.status);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_cstring(m.game_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FriendAddAck& m) {
    w.begin_bnet_packet(kSidFriendAdd);
    w.write_cstring(m.name);
    w.write_le<std::uint8_t>(m.status);
    w.write_le<std::uint8_t>(m.location);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_cstring(m.location_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FriendDelAck& m) {
    w.begin_bnet_packet(kSidFriendDel);
    w.write_le<std::uint8_t>(m.friend_num);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FriendMoveAck& m) {
    w.begin_bnet_packet(kSidFriendMove);
    w.write_le<std::uint8_t>(m.pos1);
    w.write_le<std::uint8_t>(m.pos2);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamFriendScreenRequest&) {
    w.begin_bnet_packet(kSidArrangedTeamFriendScreen);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamFriendScreenReply& m) {
    w.begin_bnet_packet(kSidArrangedTeamFriendScreen);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.names.size()));
    for (const auto& s : m.names) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamInviteFriendRequest& m) {
    w.begin_bnet_packet(kSidArrangedTeamInviteFriend);
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.friends.size()));
    for (const auto& s : m.friends) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamInviteFriendAck& m) {
    w.begin_bnet_packet(kSidArrangedTeamInviteFriend);
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.timestamp);
    w.write_le<std::uint8_t>(m.team_size);
    for (auto v : m.info) w.write_le<std::uint32_t>(v);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamMemberDecline& m) {
    w.begin_bnet_packet(kSidArrangedTeamMemberDecline);
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.action);
    w.write_cstring(m.decliner_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamSendInvite& m) {
    w.begin_bnet_packet(kSidArrangedTeamSendInvite);
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.inviter_ip);
    w.write_le<std::uint16_t>(m.port);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.other_names.size()));
    w.write_cstring(m.inviter_name);
    for (const auto& s : m.other_names) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamAcceptDeclineInvite& m) {
    w.begin_bnet_packet(kSidArrangedTeamSendInvite);
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.option);
    w.write_cstring(m.inviter_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ArrangedTeamAcceptInvite&) {
    w.begin_bnet_packet(kSidArrangedTeamAcceptInvite);
    return w.finalize_bnet_packet();
}


} // namespace pvpgn::protocol::bnet
