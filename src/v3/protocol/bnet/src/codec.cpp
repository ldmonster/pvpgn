// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/bnet/codec.hpp"

#include <string>

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::bnet {

namespace {

core::Failure<core::Error> unimplemented(std::uint8_t code) {
    std::string msg = "bnet codec: unimplemented SID 0x";
    static constexpr char kDigits[] = "0123456789ABCDEF";
    msg.push_back(kDigits[(code >> 4) & 0x0F]);
    msg.push_back(kDigits[code & 0x0F]);
    return core::fail(
        core::Error{core::StatusCode::Unimplemented, std::move(msg)});
}

#define RD_U32(target)                                                         \
    do {                                                                       \
        auto v = r.read_le<std::uint32_t>();                                   \
        if (!v) return core::fail(v.error());                                  \
        (target) = v.value();                                                  \
    } while (0)

#define RD_STR(target)                                                         \
    do {                                                                       \
        auto s = r.read_cstring();                                             \
        if (!s) return core::fail(s.error());                                  \
        (target).assign(s.value());                                            \
    } while (0)

core::Status<> check_empty_body(const Packet& pkt) {
    if (!pkt.payload.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: SID_NULL must have empty body"});
    }
    return core::ok();
}

core::Result<Ping> decode_ping(const Packet& pkt) {
    Reader r{pkt.payload};
    auto v = r.read_le<std::uint32_t>();
    if (!v) return core::fail(v.error());
    return Ping{v.value()};
}

core::Result<AuthInfo> decode_auth_info(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthInfo m;
    RD_U32(m.protocol_id);
    RD_U32(m.platform_id);
    RD_U32(m.game_id);
    RD_U32(m.version_id);
    RD_U32(m.language_id);
    RD_U32(m.local_ip);
    RD_U32(m.tz_bias);
    RD_U32(m.mpq_locale);
    RD_U32(m.lang_id);
    RD_STR(m.country_abbr);
    RD_STR(m.country);
    return m;
}

core::Result<AuthCheckReply> decode_auth_check_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthCheckReply m;
    RD_U32(m.result);
    RD_STR(m.info);
    return m;
}

core::Result<AuthInfoReply> decode_auth_info_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthInfoReply m;
    RD_U32(m.logontype);
    RD_U32(m.server_token);
    RD_U32(m.session_num);
    // u64 timestamp on the wire is little-endian (FILETIME low/high).
    std::uint32_t ts_lo = 0;
    std::uint32_t ts_hi = 0;
    RD_U32(ts_lo);
    RD_U32(ts_hi);
    m.timestamp = (static_cast<std::uint64_t>(ts_hi) << 32) | ts_lo;
    RD_STR(m.mpq_filename);
    RD_STR(m.checksum_formula);
    // Any remaining bytes are the optional W3 server-signature
    // placeholder (legacy emits 128 zero bytes for W3/W3XP, nothing
    // for other clients). Store opaquely.
    if (r.remaining() > 0) {
        auto bv = r.tail();
        m.server_signature.resize(bv.size());
        for (std::size_t i = 0; i < bv.size(); ++i) {
            m.server_signature[i] = static_cast<std::uint8_t>(bv[i]);
        }
    }
    return m;
}

core::Result<LogonResponse2> decode_logon_response2(const Packet& pkt) {
    Reader r{pkt.payload};
    LogonResponse2 m;
    RD_U32(m.client_token);
    RD_U32(m.server_token);
    for (auto& word : m.password_hash) RD_U32(word);
    RD_STR(m.username);
    return m;
}

core::Result<LogonResponse2Reply> decode_logon_response2_reply(
    const Packet& pkt) {
    Reader r{pkt.payload};
    LogonResponse2Reply m;
    RD_U32(m.result);
    // Reason is only present on result 0x06 (closed). Be tolerant.
    if (!r.empty()) RD_STR(m.reason);
    return m;
}

core::Result<JoinChannel> decode_join_channel(const Packet& pkt) {
    Reader r{pkt.payload};
    JoinChannel m;
    RD_U32(m.flags);
    RD_STR(m.channel);
    return m;
}

core::Result<EnterChatRequest> decode_enter_chat_req(const Packet& pkt) {
    Reader r{pkt.payload};
    EnterChatRequest m;
    RD_STR(m.username);
    RD_STR(m.statstring);
    return m;
}

core::Result<EnterChatReply> decode_enter_chat_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    EnterChatReply m;
    RD_STR(m.unique_name);
    RD_STR(m.statstring);
    RD_STR(m.account);
    return m;
}

core::Result<ChatCommand> decode_chat_command(const Packet& pkt) {
    Reader r{pkt.payload};
    ChatCommand m;
    RD_STR(m.text);
    return m;
}

core::Result<ChatEvent> decode_chat_event(const Packet& pkt) {
    Reader r{pkt.payload};
    ChatEvent m;
    RD_U32(m.event_id);
    RD_U32(m.flags);
    RD_U32(m.ping_ms);
    RD_U32(m.user_ip);
    RD_U32(m.acct_number);
    RD_U32(m.registration);
    RD_STR(m.username);
    RD_STR(m.text);
    return m;
}

core::Result<AuthCheckRequest> decode_auth_check_request(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthCheckRequest m;
    RD_U32(m.ticks);
    RD_U32(m.gameversion);
    RD_U32(m.checksum);
    std::uint32_t cdkey_count = 0;
    RD_U32(cdkey_count);
    RD_U32(m.spawn);
    // Bound cdkey count to prevent absurd allocations on malformed input.
    if (cdkey_count > 8u) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "bnet codec: AUTH_CHECK cdkey_count > 8"));
    }
    m.cdkeys.resize(cdkey_count);
    for (auto& key : m.cdkeys) {
        RD_U32(key.public_value);
        RD_U32(key.product);
        RD_U32(key.checksum);
        RD_U32(key.unknown);
        for (auto& word : key.hash) RD_U32(word);
    }
    RD_STR(m.exe_info);
    RD_STR(m.cdkey_owner);
    return m;
}

core::Result<GameListRequest> decode_game_list_req(const Packet& pkt) {
    Reader r{pkt.payload};
    GameListRequest m;
    auto gt = r.read_le<std::uint16_t>();
    if (!gt) return core::fail(gt.error());
    m.gametype = gt.value();
    auto u1 = r.read_le<std::uint16_t>();
    if (!u1) return core::fail(u1.error());
    m.unknown1 = u1.value();
    RD_U32(m.unknown2);
    RD_U32(m.unknown3);
    RD_U32(m.max_games);
    RD_STR(m.game_name);
    return m;
}

core::Result<GameListReply> decode_game_list_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    GameListReply m;
    std::uint32_t game_count = 0;
    RD_U32(game_count);
    RD_U32(m.sstatus);
    // Cap entries; legacy protocol uses small lists. Defensive bound.
    if (game_count > 1024u) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "bnet codec: GAMELISTREPLY game_count > 1024"));
    }
    m.entries.resize(game_count);
    for (auto& e : m.entries) {
        auto gt = r.read_le<std::uint16_t>();
        if (!gt) return core::fail(gt.error());
        e.gametype = gt.value();
        auto u1 = r.read_le<std::uint16_t>();
        if (!u1) return core::fail(u1.error());
        e.unknown1 = u1.value();
        auto u3 = r.read_le<std::uint16_t>();
        if (!u3) return core::fail(u3.error());
        e.unknown3 = u3.value();
        // port + game_ip are big-endian on the wire.
        auto pt = r.read_be<std::uint16_t>();
        if (!pt) return core::fail(pt.error());
        e.port = pt.value();
        auto ip = r.read_be<std::uint32_t>();
        if (!ip) return core::fail(ip.error());
        e.game_ip = ip.value();
        RD_U32(e.unknown4);
        RD_U32(e.unknown5);
        RD_U32(e.status);
        RD_U32(e.unknown6);
        RD_STR(e.game_name);
        RD_STR(e.password);
        RD_STR(e.info);
    }
    return m;
}

core::Result<LadderSearchRequest> decode_ladder_search_req(const Packet& pkt) {
    Reader r{pkt.payload};
    LadderSearchRequest m;
    RD_U32(m.client_tag);
    RD_U32(m.id);
    RD_U32(m.type);
    RD_STR(m.player_name);
    return m;
}

core::Result<LadderSearchReply> decode_ladder_search_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    LadderSearchReply m;
    RD_U32(m.rank);
    return m;
}

core::Result<FileInfoRequest> decode_file_info_req(const Packet& pkt) {
    Reader r{pkt.payload};
    FileInfoRequest m;
    RD_U32(m.type);
    RD_U32(m.unknown2);
    RD_STR(m.filename);
    return m;
}

core::Result<FileInfoReply> decode_file_info_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    FileInfoReply m;
    RD_U32(m.type);
    RD_U32(m.unknown2);
    auto ts = r.read_le<std::uint64_t>();
    if (!ts) return core::fail(ts.error());
    m.timestamp = ts.value();
    RD_STR(m.filename);
    return m;
}

// --- SID_CDKEY2 (0x36) ----------------------------------------------------

core::Result<CdKey2Request> decode_cdkey2_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CdKey2Request m;
    RD_U32(m.spawn);
    RD_U32(m.keylen);
    RD_U32(m.product_id);
    RD_U32(m.key_value);
    RD_U32(m.server_token);
    RD_U32(m.ticks);
    for (auto& word : m.key_hash) RD_U32(word);
    RD_STR(m.owner);
    return m;
}

core::Result<CdKey2Reply> decode_cdkey2_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CdKey2Reply m;
    RD_U32(m.result);
    // Owner echo is only present on INUSE; tolerate either way.
    if (!r.empty()) RD_STR(m.owner);
    return m;
}

// --- SID_FRIENDSLIST (0x65) -----------------------------------------------

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

core::Result<StartGame4Request> decode_startgame4_request(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame4Request m;
    auto st = r.read_le<std::uint16_t>();
    if (!st) return core::fail(st.error());
    m.status = st.value();
    auto fl = r.read_le<std::uint16_t>();
    if (!fl) return core::fail(fl.error());
    m.flag = fl.value();
    RD_U32(m.unknown2);
    auto gt = r.read_le<std::uint16_t>();
    if (!gt) return core::fail(gt.error());
    m.gametype = gt.value();
    auto op = r.read_le<std::uint16_t>();
    if (!op) return core::fail(op.error());
    m.option = op.value();
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_STR(m.game_name);
    auto pw = r.read_cstring();
    if (!pw) return core::fail(pw.error());
    m.password = pw.value();
    auto info = r.read_cstring();
    if (!info) return core::fail(info.error());
    m.info = info.value();
    return m;
}

core::Result<StartGame4Ack> decode_startgame4_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame4Ack m;
    RD_U32(m.reply);
    return m;
}

// --- SID_UDPPINGRESPONSE (0x14) -------------------------------------------

core::Result<UdpOk> decode_udp_ok(const Packet& pkt) {
    Reader r{pkt.payload};
    UdpOk m;
    RD_U32(m.echo);
    return m;
}

// --- SID_GETLADDERDATA (0x2E) ---------------------------------------------

constexpr std::uint32_t kLadderListLimit = 1024u;

core::Result<LadderListRequest> decode_ladder_list_request(const Packet& pkt) {
    Reader r{pkt.payload};
    LadderListRequest m;
    RD_U32(m.client_tag);
    RD_U32(m.id);
    RD_U32(m.type);
    RD_U32(m.start);
    RD_U32(m.count);
    return m;
}

core::Status<> read_ladder_block(Reader& r, LadderDataBlock& b) {
    auto w = r.read_le<std::uint32_t>();
    if (!w) return core::fail(w.error());
    b.wins = w.value();
    auto l = r.read_le<std::uint32_t>();
    if (!l) return core::fail(l.error());
    b.loss = l.value();
    auto d = r.read_le<std::uint32_t>();
    if (!d) return core::fail(d.error());
    b.disconnect = d.value();
    auto ra = r.read_le<std::uint32_t>();
    if (!ra) return core::fail(ra.error());
    b.rating = ra.value();
    auto rk = r.read_le<std::uint32_t>();
    if (!rk) return core::fail(rk.error());
    b.rank = rk.value();
    return core::ok();
}

core::Result<LadderListReply> decode_ladder_list_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    LadderListReply m;
    RD_U32(m.client_tag);
    RD_U32(m.id);
    RD_U32(m.type);
    RD_U32(m.start);
    RD_U32(m.count);
    if (m.count > kLadderListLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: LADDERREPLY count exceeds limit"});
    }
    m.entries.reserve(m.count);
    for (std::uint32_t i = 0; i < m.count; ++i) {
        LadderListEntry e;
        auto s = read_ladder_block(r, e.current);
        if (!s) return core::fail(s.error());
        s = read_ladder_block(r, e.active);
        if (!s) return core::fail(s.error());
        for (auto& v : e.ttest) {
            auto t = r.read_le<std::uint32_t>();
            if (!t) return core::fail(t.error());
            v = t.value();
        }
        auto lc = r.read_le<std::uint64_t>();
        if (!lc) return core::fail(lc.error());
        e.lastgame_current = lc.value();
        auto la = r.read_le<std::uint64_t>();
        if (!la) return core::fail(la.error());
        e.lastgame_active = la.value();
        auto pn = r.read_cstring();
        if (!pn) return core::fail(pn.error());
        e.player_name = pn.value();
        m.entries.emplace_back(std::move(e));
    }
    return m;
}

// --- SID_CHECKAD (0x15) ---------------------------------------------------

core::Result<AdRequest> decode_ad_request(const Packet& pkt) {
    Reader r{pkt.payload};
    AdRequest m;
    RD_U32(m.arch_tag);
    RD_U32(m.client_tag);
    RD_U32(m.prev_adid);
    RD_U32(m.ticks);
    return m;
}

core::Result<AdReply> decode_ad_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    AdReply m;
    RD_U32(m.adid);
    RD_U32(m.extension_tag);
    auto ts = r.read_le<std::uint64_t>();
    if (!ts) return core::fail(ts.error());
    m.timestamp = ts.value();
    RD_STR(m.filename);
    RD_STR(m.link);
    return m;
}

// --- CLIENT_ADCLICK (0x16) ------------------------------------------------

core::Result<AdClick> decode_ad_click(const Packet& pkt) {
    Reader r{pkt.payload};
    AdClick m;
    RD_U32(m.adid);
    RD_U32(m.unknown1);
    return m;
}

// --- CLIENT_ADACK (0x21) --------------------------------------------------

core::Result<AdAck> decode_ad_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    AdAck m;
    RD_U32(m.arch_tag);
    RD_U32(m.client_tag);
    RD_U32(m.adid);
    RD_STR(m.adfile);
    RD_STR(m.adlink);
    return m;
}

// --- CLIENT_ADCLICK2 / SERVER_ADCLICKREPLY2 (0x41) ------------------------

core::Result<AdClick2Request> decode_ad_click2_request(const Packet& pkt) {
    Reader r{pkt.payload};
    AdClick2Request m;
    RD_U32(m.adid);
    return m;
}

core::Result<AdClick2Reply> decode_ad_click2_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    AdClick2Reply m;
    RD_U32(m.adid);
    RD_STR(m.link);
    return m;
}

// --- SID_NEWS_INFO / MOTD (0x46) ------------------------------------------

core::Result<MotdRequest> decode_motd_request(const Packet& pkt) {
    Reader r{pkt.payload};
    MotdRequest m;
    RD_U32(m.last_news_time);
    return m;
}

core::Result<MotdReply> decode_motd_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    MotdReply m;
    auto mt = r.read_le<std::uint8_t>();
    if (!mt) return core::fail(mt.error());
    m.msg_type = mt.value();
    RD_U32(m.curr_time);
    RD_U32(m.first_news_time);
    RD_U32(m.timestamp);
    RD_U32(m.timestamp2);
    RD_STR(m.text);
    return m;
}

// --- CLIENT_PROGIDENT2 / SERVER_CHANNELLIST (0x0B) ------------------------

core::Result<ChannelListRequest> decode_channel_list_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ChannelListRequest m;
    RD_U32(m.client_tag);
    return m;
}

constexpr std::size_t kChannelListLimit = 1024u;

core::Result<ChannelListReply> decode_channel_list_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ChannelListReply m;
    while (!r.empty()) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        if (s.value().empty()) break;  // terminator
        if (m.channels.size() >= kChannelListLimit) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "bnet codec: CHANNELLIST exceeds limit"});
        }
        m.channels.emplace_back(s.value());
    }
    return m;
}

// --- CLIENT_LEAVECHANNEL (0x10) ------------------------------------------

core::Result<LeaveChannel> decode_leave_channel(const Packet& pkt) {
    if (!pkt.payload.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: LEAVECHANNEL must have empty body"});
    }
    return LeaveChannel{};
}

// --- SERVER_REGSNOOPREQ / CLIENT_REGSNOOPREPLY (0x18) --------------------

core::Result<RegSnoopRequest> decode_regsnoop_request(const Packet& pkt) {
    Reader r{pkt.payload};
    RegSnoopRequest m;
    RD_U32(m.unknown1);
    RD_U32(m.hkey);
    RD_STR(m.reg_key);
    RD_STR(m.value_name);
    return m;
}

core::Result<RegSnoopReply> decode_regsnoop_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    RegSnoopReply m;
    RD_U32(m.unknown1);
    auto tail = r.tail();
    m.value.assign(tail.begin(), tail.end());
    return m;
}

// --- SID_PROFILE (0x35) ---------------------------------------------------

core::Result<ProfileRequest> decode_profile_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ProfileRequest m;
    RD_U32(m.cookie);
    RD_STR(m.player_name);
    return m;
}

core::Result<ProfileReply> decode_profile_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ProfileReply m;
    RD_U32(m.cookie);
    auto f = r.read_le<std::uint8_t>();
    if (!f) return core::fail(f.error());
    m.fail = f.value();
    if (m.fail == 0) {
        RD_STR(m.description);
        RD_STR(m.location);
        // clan_tag trailer is optional (older builds omit it). Read if
        // present, otherwise leave at default 0.
        if (r.remaining() >= sizeof(std::uint32_t)) {
            RD_U32(m.clan_tag);
        }
    }
    return m;
}

// --- SID_SETEMAIL (0x59) --------------------------------------------------

core::Result<SetEmailRequest> decode_setemail_request(const Packet& pkt) {
    if (!pkt.payload.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: SETEMAILREQ must have empty body"});
    }
    return SetEmailRequest{};
}

core::Result<SetEmailReply> decode_setemail_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    SetEmailReply m;
    RD_STR(m.email);
    return m;
}

// --- SID_ICONREQ (0x2D) ---------------------------------------------------

core::Result<IconRequest> decode_icon_request(const Packet& pkt) {
    if (!pkt.payload.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: ICONREQ must have empty body"});
    }
    return IconRequest{};
}

core::Result<IconReply> decode_icon_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    IconReply m;
    auto ts = r.read_le<std::uint64_t>();
    if (!ts) return core::fail(ts.error());
    m.timestamp = ts.value();
    RD_STR(m.filename);
    return m;
}

// --- SID_GETPASSWORD (0x5A) -----------------------------------------------

core::Result<GetPasswordRequest> decode_get_password_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    GetPasswordRequest m;
    RD_STR(m.account_name);
    RD_STR(m.email);
    return m;
}

// --- SID_CHANGEEMAIL (0x5B) -----------------------------------------------

core::Result<ChangeEmailRequest> decode_change_email_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ChangeEmailRequest m;
    RD_STR(m.account_name);
    RD_STR(m.old_email);
    RD_STR(m.new_email);
    return m;
}

// --- SID_CRASHDUMP (0x5D) -------------------------------------------------

core::Result<CrashDump> decode_crash_dump(const Packet& pkt) {
    CrashDump m;
    m.data.assign(pkt.payload.begin(), pkt.payload.end());
    return m;
}

// --- SID_UNKNOWN_37 (0x37) — legacy D2 character list ---------------------

core::Result<CharListRequest> decode_char_list_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CharListRequest m;
    RD_U32(m.open_count);
    auto t = r.tail();
    m.char_data.assign(t.begin(), t.end());
    return m;
}

core::Result<CharListReply> decode_char_list_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CharListReply m;
    RD_U32(m.unknown1);
    RD_U32(m.max_chars);
    RD_U32(m.count);
    auto t = r.tail();
    m.char_data.assign(t.begin(), t.end());
    return m;
}

// --- SID_SERVERLIST (0x04) -----------------------------------------------

core::Result<ServerList> decode_server_list(const Packet& pkt) {
    Reader r{pkt.payload};
    ServerList m;
    RD_U32(m.unknown1);
    RD_STR(m.servers);
    return m;
}

// --- SID_MESSAGEBOX (0x19) -----------------------------------------------

core::Result<MessageBox> decode_message_box(const Packet& pkt) {
    Reader r{pkt.payload};
    MessageBox m;
    RD_U32(m.style);
    RD_STR(m.text);
    RD_STR(m.caption);
    return m;
}

// --- SID_REALMLIST_110 (0x40) -------------------------------------------

constexpr std::uint32_t kRealmListLimit = 256u;

core::Result<RealmListRequest> decode_realm_list_request(const Packet&) {
    return RealmListRequest{};
}

core::Result<RealmListReply> decode_realm_list_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    RealmListReply m;
    RD_U32(m.unknown1);
    std::uint32_t count = 0;
    RD_U32(count);
    if (count > kRealmListLimit) {
        return core::fail(core::Error{core::StatusCode::OutOfRange,
                                      "REALMLISTREPLY count exceeds limit"});
    }
    m.entries.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        RealmListEntry e;
        RD_U32(e.unknown);
        RD_STR(e.name);
        RD_STR(e.description);
        m.entries.push_back(std::move(e));
    }
    return m;
}

// --- SID_REALMJOIN_109 (0x3E) -------------------------------------------

core::Result<RealmJoinRequest> decode_realm_join_request(const Packet& pkt) {
    Reader r{pkt.payload};
    RealmJoinRequest m;
    RD_U32(m.seqno);
    for (auto& w : m.seqno_hash) {
        auto v = r.read_le<std::uint32_t>();
        if (!v) return core::fail(v.error());
        w = v.value();
    }
    RD_STR(m.realm_name);
    return m;
}

core::Result<RealmJoinReply> decode_realm_join_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    RealmJoinReply m;
    RD_U32(m.seqno);
    RD_U32(m.u1);
    RD_U32(m.bncs_addr1);
    RD_U32(m.session_num);
    RD_U32(m.addr);
    {
        auto v = r.read_be<std::uint16_t>();
        if (!v) return core::fail(v.error());
        m.port = v.value();
    }
    {
        auto v = r.read_le<std::uint16_t>();
        if (!v) return core::fail(v.error());
        m.u3 = v.value();
    }
    RD_U32(m.session_key);
    RD_U32(m.u5);
    RD_U32(m.u6);
    RD_U32(m.client_tag);
    RD_U32(m.version_id);
    RD_U32(m.bncs_addr2);
    RD_U32(m.u7);
    for (auto& w : m.secret_hash) {
        auto v = r.read_le<std::uint32_t>();
        if (!v) return core::fail(v.error());
        w = v.value();
    }
    RD_STR(m.account_name);
    return m;
}

// --- SID_WARCRAFTGENERAL (0x44) -----------------------------------------

core::Result<WarcraftGeneralRequest> decode_warcraft_general_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    WarcraftGeneralRequest m;
    auto opt = r.read_le<std::uint8_t>();
    if (!opt) return core::fail(opt.error());
    m.sub_option = opt.value();
    auto t = r.tail();
    m.data.assign(t.begin(), t.end());
    return m;
}

core::Result<WarcraftGeneralReply> decode_warcraft_general_reply(
    const Packet& pkt) {
    Reader r{pkt.payload};
    WarcraftGeneralReply m;
    auto opt = r.read_le<std::uint8_t>();
    if (!opt) return core::fail(opt.error());
    m.sub_option = opt.value();
    auto t = r.tail();
    m.data.assign(t.begin(), t.end());
    return m;
}

// --- SID_REQUIREDWORK (0x4C) / SID_EXTRAWORK (0x4B) ----------------------

constexpr std::uint16_t kExtraWorkMaxLen = 32768u;

core::Result<RequiredWork> decode_required_work(const Packet& pkt) {
    Reader r{pkt.payload};
    RequiredWork m;
    RD_STR(m.filename);
    return m;
}

core::Result<ExtraWork> decode_extra_work(const Packet& pkt) {
    Reader r{pkt.payload};
    ExtraWork m;
    auto gt = r.read_le<std::uint16_t>();
    if (!gt) return core::fail(gt.error());
    m.game_type = gt.value();
    auto len = r.read_le<std::uint16_t>();
    if (!len) return core::fail(len.error());
    if (len.value() > kExtraWorkMaxLen) {
        return core::fail(core::Error{core::StatusCode::OutOfRange,
                                      "EXTRAWORK length exceeds limit"});
    }
    auto blob = r.read_bytes(len.value());
    if (!blob) return core::fail(blob.error());
    m.data.assign(blob.value().begin(), blob.value().end());
    return m;
}

// --- SID_REALMLIST (0x34) — pre-1.10 ------------------------------------

core::Result<RealmListLegacyRequest> decode_realm_list_legacy_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    RealmListLegacyRequest m;
    RD_U32(m.unknown1);
    RD_U32(m.unknown2);
    return m;
}

core::Result<RealmListLegacyReply> decode_realm_list_legacy_reply(
    const Packet& pkt) {
    Reader r{pkt.payload};
    RealmListLegacyReply m;
    RD_U32(m.unknown1);
    std::uint32_t count = 0;
    RD_U32(count);
    if (count > kRealmListLimit) {
        return core::fail(core::Error{core::StatusCode::OutOfRange,
                                      "REALMLISTREPLY (legacy) count exceeds limit"});
    }
    m.entries.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        RealmListLegacyEntry e;
        RD_U32(e.unknown3);
        RD_U32(e.unknown4);
        RD_U32(e.unknown5);
        RD_U32(e.unknown6);
        RD_U32(e.unknown7);
        RD_U32(e.unknown8);
        RD_U32(e.unknown9);
        RD_STR(e.name);
        RD_STR(e.description);
        m.entries.push_back(std::move(e));
    }
    return m;
}

// --- SID_CDKEY3 (0x42) ---------------------------------------------------

core::Result<CdKey3Request> decode_cdkey3_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CdKey3Request m;
    RD_U32(m.unknown1);
    RD_U32(m.unknown2);
    RD_U32(m.unknown3);
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_U32(m.unknown6);
    RD_U32(m.unknown7);
    for (auto& w : m.key_hash) {
        auto v = r.read_le<std::uint32_t>();
        if (!v) return core::fail(v.error());
        w = v.value();
    }
    RD_STR(m.owner_name);
    return m;
}

core::Result<CdKey3Reply> decode_cdkey3_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CdKey3Reply m;
    RD_U32(m.message);
    if (!r.empty()) {
        RD_STR(m.owner_name);
    }
    return m;
}

// --- SID_CREATEACCOUNT2 (0x52) -------------------------------------------

core::Result<CreateAccount2Request> decode_createaccount2_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccount2Request m;
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.password_verifier.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.password_verifier.size(); ++i)
            m.password_verifier[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    RD_STR(m.account_name);
    return m;
}

core::Result<CreateAccount2Reply> decode_createaccount2_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccount2Reply m;
    RD_U32(m.result);
    return m;
}

// --- SID_LOGINREQ_W3 / SID_LOGINREPLY_W3 (0x53) -------------------------

core::Result<LoginW3Request> decode_loginw3_request(const Packet& pkt) {
    Reader r{pkt.payload};
    LoginW3Request m;
    {
        auto v = r.read_bytes(m.client_public_key.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.client_public_key.size(); ++i)
            m.client_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    RD_STR(m.account_name);
    return m;
}

core::Result<LoginW3Reply> decode_loginw3_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    LoginW3Reply m;
    RD_U32(m.message);
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.server_public_key.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.server_public_key.size(); ++i)
            m.server_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    return m;
}

// --- SID_LOGONPROOFREQ / SID_LOGONPROOFREPLY (0x54) ---------------------

core::Result<LogonProofW3Request> decode_logonproof_w3_request(const Packet& pkt) {
    Reader r{pkt.payload};
    LogonProofW3Request m;
    auto v = r.read_bytes(m.client_password_proof.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.client_password_proof.size(); ++i)
        m.client_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    return m;
}

core::Result<LogonProofW3Reply> decode_logonproof_w3_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    LogonProofW3Reply m;
    RD_U32(m.response);
    auto v = r.read_bytes(m.server_password_proof.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.server_password_proof.size(); ++i)
        m.server_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    if (!r.empty()) {
        RD_STR(m.message);
    }
    return m;
}

// --- SID_PASSCHANGEREQ / SID_PASSCHANGEREPLY (0x55) ---------------------

core::Result<PassChangeRequest> decode_passchange_request(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeRequest m;
    auto v = r.read_bytes(m.client_public_key.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.client_public_key.size(); ++i)
        m.client_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    RD_STR(m.account_name);
    return m;
}

core::Result<PassChangeReply> decode_passchange_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeReply m;
    RD_U32(m.message);
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.server_public_key.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.server_public_key.size(); ++i)
            m.server_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    return m;
}

// --- SID_PASSCHANGEPROOFREQ / SID_PASSCHANGEPROOFREPLY (0x56) ----------

core::Result<PassChangeProofRequest> decode_passchange_proof_request(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeProofRequest m;
    {
        auto v = r.read_bytes(m.client_password_proof.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.client_password_proof.size(); ++i)
            m.client_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.password_verifier.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.password_verifier.size(); ++i)
            m.password_verifier[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    return m;
}

core::Result<PassChangeProofReply> decode_passchange_proof_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeProofReply m;
    RD_U32(m.response);
    auto v = r.read_bytes(m.server_password_proof.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.server_password_proof.size(); ++i)
        m.server_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    return m;
}

// --- SID_CLANINFO (0x82) ---------------------------------------------------

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
core::Status<> read_strings(Reader& r, std::vector<std::string>& out,
                            std::uint32_t count) {
    constexpr std::uint32_t kLimit = 256u;  // defensive
    if (count > kLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: userdata count exceeds limit"});
    }
    out.clear();
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        out.emplace_back(s.value());
    }
    return core::ok();
}

core::Result<UserDataReadRequest> decode_userdata_read_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    UserDataReadRequest m;
    std::uint32_t name_count = 0;
    std::uint32_t key_count  = 0;
    RD_U32(name_count);
    RD_U32(key_count);
    RD_U32(m.request_id);
    auto s = read_strings(r, m.names, name_count);
    if (!s) return core::fail(s.error());
    s = read_strings(r, m.keys, key_count);
    if (!s) return core::fail(s.error());
    return m;
}

core::Result<UserDataReadReply> decode_userdata_read_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    UserDataReadReply m;
    RD_U32(m.name_count);
    RD_U32(m.key_count);
    RD_U32(m.request_id);
    // name_count * key_count must fit in u32 and stay below a defensive cap.
    constexpr std::uint64_t kCellLimit = 4096u;
    const std::uint64_t cells =
        static_cast<std::uint64_t>(m.name_count) *
        static_cast<std::uint64_t>(m.key_count);
    if (cells > kCellLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: userdata read reply cell count exceeds limit"});
    }
    auto s = read_strings(r, m.values, static_cast<std::uint32_t>(cells));
    if (!s) return core::fail(s.error());
    return m;
}

core::Result<UserDataWriteRequest> decode_userdata_write_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    UserDataWriteRequest m;
    std::uint32_t name_count = 0;
    std::uint32_t key_count  = 0;
    RD_U32(name_count);
    RD_U32(key_count);
    auto s = read_strings(r, m.names, name_count);
    if (!s) return core::fail(s.error());
    s = read_strings(r, m.keys, key_count);
    if (!s) return core::fail(s.error());
    constexpr std::uint64_t kCellLimit = 4096u;
    const std::uint64_t cells =
        static_cast<std::uint64_t>(name_count) *
        static_cast<std::uint64_t>(key_count);
    if (cells > kCellLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: userdata write request cell count exceeds limit"});
    }
    s = read_strings(r, m.values, static_cast<std::uint32_t>(cells));
    if (!s) return core::fail(s.error());
    return m;
}

// --- SID_CLAN_CREATE family (0x70..0x7C) ----------------------------------

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
decode_clan_member_rankupdate_request(const Packet& pkt) {
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
core::Result<CompInfo1Request> decode_compinfo1_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CompInfo1Request m;
    RD_U32(m.reg_version);
    RD_U32(m.reg_auth);
    RD_U32(m.client_id);
    RD_U32(m.client_token);
    if (!r.empty()) { RD_STR(m.host); }
    if (!r.empty()) { RD_STR(m.user); }
    return m;
}
// 0x05 SERVER_COMPREPLY
core::Result<CompReply> decode_compreply(const Packet& pkt) {
    Reader r{pkt.payload};
    CompReply m;
    RD_U32(m.reg_version);
    RD_U32(m.reg_auth);
    RD_U32(m.client_id);
    RD_U32(m.client_token);
    return m;
}
// 0x06 CLIENT_PROGIDENT
core::Result<ProgIdent> decode_progident(const Packet& pkt) {
    Reader r{pkt.payload};
    ProgIdent m;
    RD_U32(m.archtag);
    RD_U32(m.clienttag);
    RD_U32(m.versionid);
    RD_U32(m.unknown1);
    return m;
}
// 0x06 SERVER_AUTHREQ1
core::Result<AuthReq1Server> decode_authreq1_server(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthReq1Server m;
    RD_U64(m.timestamp);
    RD_STR(m.filename);
    RD_STR(m.equation);
    return m;
}
// 0x07 CLIENT_AUTHREQ1
core::Result<AuthReq1> decode_authreq1(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthReq1 m;
    RD_U32(m.archtag);
    RD_U32(m.clienttag);
    RD_U32(m.versionid);
    RD_U32(m.gameversion);
    RD_U32(m.checksum);
    RD_STR(m.exeinfo);
    return m;
}
// 0x07 SERVER_AUTHREPLY1
core::Result<AuthReply1> decode_authreply1(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthReply1 m;
    RD_U32(m.message);
    // Legacy always emits at least two trailing NUL-terminated
    // strings. Materialise the first as `filename` when it is
    // non-empty; the trailing empties are consumed but ignored.
    if (!r.empty()) {
        auto cs = r.read_cstring();
        if (!cs) return core::fail(cs.error());
        if (!cs.value().empty()) m.filename.assign(cs.value());
    }
    if (!r.empty()) {
        auto cs = r.read_cstring();
        if (!cs) return core::fail(cs.error());
    }
    return m;
}
// 0x12 CLIENT_COUNTRYINFO1
core::Result<CountryInfo1> decode_countryinfo1(const Packet& pkt) {
    Reader r{pkt.payload};
    CountryInfo1 m;
    RD_U64(m.systemtime);
    RD_U64(m.localtime);
    {
        auto v = r.read_le<std::uint32_t>();
        if (!v) return core::fail(v.error());
        m.bias = static_cast<std::int32_t>(v.value());
    }
    RD_U32(m.langid1);
    RD_U32(m.langid2);
    RD_U32(m.langid3);
    RD_STR(m.langstr);
    RD_STR(m.countrycode);
    RD_STR(m.countryabbrev);
    RD_STR(m.countryname);
    return m;
}
// 0x1D SERVER_SESSIONKEY2
core::Result<SessionKey2> decode_sessionkey2(const Packet& pkt) {
    Reader r{pkt.payload};
    SessionKey2 m;
    RD_U32(m.sessionnum);
    RD_U32(m.sessionkey);
    return m;
}
// 0x1E CLIENT_COMPINFO2
core::Result<CompInfo2> decode_compinfo2(const Packet& pkt) {
    Reader r{pkt.payload};
    CompInfo2 m;
    RD_U32(m.unknown1);
    RD_U32(m.reg_version);
    RD_U32(m.reg_auth);
    RD_U32(m.client_id);
    RD_U32(m.client_token);
    if (!r.empty()) { RD_STR(m.host); }
    if (!r.empty()) { RD_STR(m.user); }
    return m;
}
// 0x28 SERVER_SESSIONKEY1
core::Result<SessionKey1> decode_sessionkey1(const Packet& pkt) {
    Reader r{pkt.payload};
    SessionKey1 m;
    RD_U32(m.sessionkey);
    return m;
}
// 0x29 CLIENT_LOGINREQ1
core::Result<LoginReq1> decode_loginreq1(const Packet& pkt) {
    Reader r{pkt.payload};
    LoginReq1 m;
    RD_U32(m.ticks);
    RD_U32(m.sessionkey);
    RD_HASH5(m.password_hash2);
    RD_STR(m.player_name);
    return m;
}
// 0x29 SERVER_LOGINREPLY1
core::Result<LoginReply1> decode_loginreply1(const Packet& pkt) {
    Reader r{pkt.payload};
    LoginReply1 m;
    RD_U32(m.message);
    return m;
}
// 0x2A CLIENT_CREATEACCTREQ1
core::Result<CreateAccount1Request> decode_createaccount1_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccount1Request m;
    RD_HASH5(m.password_hash1);
    RD_STR(m.player_name);
    return m;
}
// 0x2A SERVER_CREATEACCTREPLY1
core::Result<CreateAccount1Reply> decode_createaccount1_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccount1Reply m;
    RD_U32(m.result);
    return m;
}
// 0x2B CLIENT_UNKNOWN_2B
core::Result<Unknown2B> decode_unknown_2b(const Packet& pkt) {
    Reader r{pkt.payload};
    Unknown2B m;
    RD_U32(m.unknown1);
    RD_U32(m.unknown2);
    RD_U32(m.unknown3);
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_U32(m.unknown6);
    RD_U32(m.unknown7);
    return m;
}
// 0x30 CLIENT_CDKEY
core::Result<CdKeyLegacyRequest> decode_cdkey_legacy_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CdKeyLegacyRequest m;
    RD_U32(m.spawn);
    RD_STR(m.cdkey);
    if (!r.empty()) { RD_STR(m.owner_name); }
    return m;
}
// 0x30 SERVER_CDKEYREPLY
core::Result<CdKeyLegacyReply> decode_cdkey_legacy_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CdKeyLegacyReply m;
    RD_U32(m.message);
    if (!r.empty()) { RD_STR(m.owner_name); }
    return m;
}
// 0x31 CLIENT_CHANGEPASSREQ
core::Result<ChangePasswordRequest> decode_changepassword_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ChangePasswordRequest m;
    RD_U32(m.ticks);
    RD_U32(m.sessionkey);
    RD_HASH5(m.oldpassword_hash2);
    RD_HASH5(m.newpassword_hash1);
    RD_STR(m.player_name);
    return m;
}
// 0x31 SERVER_CHANGEPASSACK
core::Result<ChangePasswordReply> decode_changepassword_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ChangePasswordReply m;
    RD_U32(m.message);
    return m;
}
// 0x39 CLIENT_UNKNOWN_39
core::Result<Unknown39> decode_unknown_39(const Packet& pkt) {
    Reader r{pkt.payload};
    Unknown39 m;
    RD_STR(m.char_name);
    return m;
}
// 0x3D CLIENT_CREATEACCTREQ2
core::Result<CreateAccountRequest> decode_createaccount_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccountRequest m;
    RD_HASH5(m.password_hash1);
    RD_STR(m.username);
    return m;
}
// 0x3D SERVER_CREATEACCTREPLY2
core::Result<CreateAccountReply> decode_createaccount_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccountReply m;
    RD_U32(m.result);
    return m;
}
// 0x45 CLIENT_CHANGEGAMEPORT
core::Result<NetGamePort> decode_netgameport(const Packet& pkt) {
    Reader r{pkt.payload};
    NetGamePort m;
    RD_U16(m.port);
    return m;
}

// --- Game-lifecycle decoders --------------------------------------------

// 0x02 CLIENT_CLOSEGAME / 0x1F CLIENT_CLOSEGAME2 — empty body.
core::Result<CloseGame> decode_close_game(const Packet& pkt) {
    auto s = check_empty_body(pkt);
    if (!s) return core::fail(s.error());
    return CloseGame{};
}
core::Result<CloseGame2> decode_close_game2(const Packet& pkt) {
    auto s = check_empty_body(pkt);
    if (!s) return core::fail(s.error());
    return CloseGame2{};
}

// 0x08 CLIENT_STARTGAME1
core::Result<StartGame1Request> decode_startgame1_request(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame1Request m;
    RD_U32(m.status);
    RD_U32(m.unknown3);
    RD_U16(m.gametype);
    RD_U16(m.unknown1);
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_STR(m.game_name);
    RD_STR(m.password);
    RD_STR(m.info);
    return m;
}
core::Result<StartGame1Ack> decode_startgame1_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame1Ack m;
    RD_U32(m.reply);
    return m;
}

// 0x1A CLIENT_STARTGAME3
core::Result<StartGame3Request> decode_startgame3_request(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame3Request m;
    RD_U32(m.status);
    RD_U32(m.unknown3);
    RD_U16(m.gametype);
    RD_U16(m.unknown1);
    RD_U32(m.unknown6);
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_STR(m.game_name);
    RD_STR(m.password);
    RD_STR(m.info);
    return m;
}
core::Result<StartGame3Ack> decode_startgame3_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame3Ack m;
    RD_U32(m.reply);
    return m;
}

// 0x22 CLIENT_JOIN_GAME
core::Result<JoinGame> decode_join_game(const Packet& pkt) {
    Reader r{pkt.payload};
    JoinGame m;
    RD_U32(m.clienttag);
    RD_U32(m.versiontag);
    RD_STR(m.game_name);
    RD_STR(m.password);
    return m;
}

// 0x2C CLIENT_GAME_REPORT
core::Result<GameReport> decode_game_report(const Packet& pkt) {
    Reader r{pkt.payload};
    GameReport m;
    RD_U32(m.unknown1);
    std::uint32_t count = 0;
    RD_U32(count);
    // Bound count to remaining bytes to prevent OOM/DoS via crafted packet.
    // Each result is 4 bytes + at least 1 byte for cstring name; cap reasonably.
    if (count > r.remaining() / 4u) {
        return core::fail(core::Error{core::StatusCode::InvalidArgument,
            "bnet codec: GAMEREPORT count exceeds payload"});
    }
    m.results.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        RD_U32(m.results[i]);
    }
    m.player_names.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        m.player_names[i] = s.value();
    }
    RD_STR(m.report_header);
    RD_STR(m.report_body);
    return m;
}

// --- Misc / anti-cheat / advisory decoders ------------------------------

// 0x17 SERVER_READMEMORY: request_id, address, length.
core::Result<ReadMemoryRequest> decode_read_memory_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ReadMemoryRequest m;
    RD_U32(m.request_id);
    RD_U32(m.address);
    RD_U32(m.length);
    return m;
}

// 0x17 CLIENT_READMEMORY: request_id followed by raw memory bytes (rest of payload).
core::Result<ReadMemoryReply> decode_read_memory_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ReadMemoryReply m;
    RD_U32(m.request_id);
    auto tail = r.tail();
    m.memory.resize(tail.size());
    for (std::size_t i = 0; i < tail.size(); ++i) {
        m.memory[i] = static_cast<std::uint8_t>(tail[i]);
    }
    return m;
}

// 0x1B CLIENT_UNKNOWN_1B: u16 unknown1, u16 port_be, u32 ip_be, u32, u32.
core::Result<Unknown1B> decode_unknown_1b(const Packet& pkt) {
    Reader r{pkt.payload};
    Unknown1B m;
    RD_U16(m.unknown1);
    RD_U16(m.port_be);
    RD_U32(m.ip_be);
    RD_U32(m.unknown2);
    RD_U32(m.unknown3);
    return m;
}

// 0x24 CLIENT_UNKNOWN_24: empty body.
core::Result<Unknown24> decode_unknown_24(const Packet& pkt) {
    auto s = check_empty_body(pkt);
    if (!s) return core::fail(s.error());
    return Unknown24{};
}

// 0x32 CLIENT_MAPAUTHREQ1: 5×u32 checksum, cstring mapfile.
core::Result<MapAuthReq1> decode_mapauthreq1(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReq1 m;
    RD_HASH5(m.file_checksum);
    RD_STR(m.mapfile);
    return m;
}
core::Result<MapAuthReply1> decode_mapauthreply1(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReply1 m;
    RD_U32(m.response);
    return m;
}

// 0x3C CLIENT_MAPAUTHREQ2: u32 unknown, 5×u32 hash, cstring mapfile.
core::Result<MapAuthReq2> decode_mapauthreq2(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReq2 m;
    RD_U32(m.unknown);
    RD_HASH5(m.file_hash);
    RD_STR(m.mapfile);
    return m;
}
core::Result<MapAuthReply2> decode_mapauthreply2(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReply2 m;
    RD_U32(m.response);
    return m;
}

// 0x5C CLIENT_CHANGECLIENT: u32 clienttag.
core::Result<ChangeClient> decode_change_client(const Packet& pkt) {
    Reader r{pkt.payload};
    ChangeClient m;
    RD_U32(m.clienttag);
    return m;
}

#undef RD_U64
#undef RD_U16
#undef RD_HASH5
#undef RD_U32
#undef RD_STR

}  // namespace

core::Result<ClientMessage> decode_client(const Packet& pkt) {
    switch (pkt.header.code) {
        case kSidNull: {
            auto s = check_empty_body(pkt);
            if (!s) return core::fail(s.error());
            return ClientMessage{Null{}};
        }
        case kSidPing: {
            auto m = decode_ping(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAuthInfo: {
            auto m = decode_auth_info(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLogonResponse2: {
            auto m = decode_logon_response2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidJoinChannel: {
            auto m = decode_join_channel(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidEnterChat: {
            auto m = decode_enter_chat_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChatCommand: {
            auto m = decode_chat_command(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAuthCheck: {
            auto m = decode_auth_check_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGetAdvListEx: {
            auto m = decode_game_list_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLadderSearch: {
            auto m = decode_ladder_search_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGetFileTime: {
            auto m = decode_file_info_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCdKey2: {
            auto m = decode_cdkey2_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidFriendsList: {
            auto m = decode_friendslist_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidFriendInfo: {
            auto m = decode_friendinfo_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanInfo: {
            auto m = decode_claninfo_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidReadUserData: {
            auto m = decode_userdata_read_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidWriteUserData: {
            auto m = decode_userdata_write_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanCreate: {
            auto m = decode_clan_create_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanDisband: {
            auto m = decode_clan_disband_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberNewChief: {
            auto m = decode_clan_newchief_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanInvite: {
            auto m = decode_clan_invite_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberRemove: {
            auto m = decode_clan_member_remove_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberRankUpdate: {
            auto m = decode_clan_member_rankupdate_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMotdChange: {
            auto m = decode_clan_motd_change(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMotd: {
            auto m = decode_clan_motd_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanCreateInvite: {
            auto m = decode_clan_create_invite_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanCreateInvite2: {
            auto m = decode_clan_create_invite_response(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanInvite2: {
            auto m = decode_clan_invite2_response(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberList: {
            auto m = decode_clan_memberlist_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamFriendScreen: {
            auto m = decode_arrangedteam_friendscreen_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamInviteFriend: {
            auto m = decode_arrangedteam_invite_friend_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamSendInvite: {
            auto m = decode_arrangedteam_accept_decline_invite(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamAcceptInvite: {
            auto m = decode_arrangedteam_accept_invite(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidStartGame4: {
            auto m = decode_startgame4_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUdpOk: {
            auto m = decode_udp_ok(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLadderList: {
            auto m = decode_ladder_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCheckAd: {
            auto m = decode_ad_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAdClick: {
            auto m = decode_ad_click(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAdAck: {
            auto m = decode_ad_ack(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAdClick2: {
            auto m = decode_ad_click2_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidMotd: {
            auto m = decode_motd_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChannelList: {
            auto m = decode_channel_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLeaveChat: {
            auto m = decode_leave_channel(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRegSnoop: {
            auto m = decode_regsnoop_reply(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidProfile: {
            auto m = decode_profile_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidSetEmail: {
            auto m = decode_setemail_reply(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidIconReq: {
            auto m = decode_icon_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGetPassword: {
            auto m = decode_get_password_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChangeEmail: {
            auto m = decode_change_email_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCrashDump: {
            auto m = decode_crash_dump(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCharList: {
            auto m = decode_char_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRealmList: {
            auto m = decode_realm_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRealmJoin: {
            auto m = decode_realm_join_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidWarcraftGeneral: {
            auto m = decode_warcraft_general_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidExtraWork: {
            auto m = decode_extra_work(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRealmListLegacy: {
            auto m = decode_realm_list_legacy_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCdKey3: {
            auto m = decode_cdkey3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCreateAccount2: {
            auto m = decode_createaccount2_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLoginW3: {
            auto m = decode_loginw3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLogonProofW3: {
            auto m = decode_logonproof_w3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidPassChange: {
            auto m = decode_passchange_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidPassChangeProof: {
            auto m = decode_passchange_proof_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCompInfo1: {
            auto m = decode_compinfo1_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidProgIdent: {
            auto m = decode_progident(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAuthReq1: {
            auto m = decode_authreq1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCountryInfo1: {
            auto m = decode_countryinfo1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCompInfo2: {
            auto m = decode_compinfo2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLogonResponse: {
            auto m = decode_loginreq1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCreateAccount1: {
            auto m = decode_createaccount1_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown2B: {
            auto m = decode_unknown_2b(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCdKeyLegacy: {
            auto m = decode_cdkey_legacy_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChangePassword: {
            auto m = decode_changepassword_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown39: {
            auto m = decode_unknown_39(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCreateAccount: {
            auto m = decode_createaccount_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidNetGamePort: {
            auto m = decode_netgameport(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCloseGame: {
            auto m = decode_close_game(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCloseGame2: {
            auto m = decode_close_game2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidStartGame1: {
            auto m = decode_startgame1_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidStartGame3: {
            auto m = decode_startgame3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidJoinGame: {
            auto m = decode_join_game(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGameReport: {
            auto m = decode_game_report(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidReadMemory: {
            auto m = decode_read_memory_reply(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown1B: {
            auto m = decode_unknown_1b(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown24: {
            auto m = decode_unknown_24(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidMapAuth1: {
            auto m = decode_mapauthreq1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidMapAuth2: {
            auto m = decode_mapauthreq2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChangeClient: {
            auto m = decode_change_client(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        default:
            return unimplemented(pkt.header.code);
    }
}

core::Result<ServerMessage> decode_server(const Packet& pkt) {
    switch (pkt.header.code) {
        case kSidNull: {
            auto s = check_empty_body(pkt);
            if (!s) return core::fail(s.error());
            return ServerMessage{Null{}};
        }
        case kSidPing: {
            auto m = decode_ping(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAuthCheck: {
            auto m = decode_auth_check_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAuthInfo: {
            auto m = decode_auth_info_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLogonResponse2: {
            auto m = decode_logon_response2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidEnterChat: {
            auto m = decode_enter_chat_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidChatEvent: {
            auto m = decode_chat_event(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidGetAdvListEx: {
            auto m = decode_game_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLadderSearch: {
            auto m = decode_ladder_search_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidGetFileTime: {
            auto m = decode_file_info_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCdKey2: {
            auto m = decode_cdkey2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendsList: {
            auto m = decode_friendslist_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendInfo: {
            auto m = decode_friendinfo_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendAdd: {
            auto m = decode_friendadd_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendDel: {
            auto m = decode_frienddel_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendMove: {
            auto m = decode_friendmove_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanInfo: {
            auto m = decode_claninfo_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidReadUserData: {
            auto m = decode_userdata_read_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanCreate: {
            auto m = decode_clan_create_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanDisband:
        case kSidClanMemberNewChief:
        case kSidClanInvite:
        case kSidClanMemberRemove:
        case kSidClanMemberRankUpdate: {
            auto m = decode_clan_generic_result_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMotd: {
            auto m = decode_clan_motd_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanCreateInvite: {
            auto m = decode_clan_create_invite_summary(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanCreateInvite2: {
            auto m = decode_clan_create_invite_forward(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanInvite2: {
            auto m = decode_clan_invite2_forward(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMemberList: {
            auto m = decode_clan_memberlist_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMemberRemoved: {
            auto m = decode_clan_member_removed_notify(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMemberUpdate: {
            auto m = decode_clan_member_update(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamFriendScreen: {
            auto m = decode_arrangedteam_friendscreen_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamInviteFriend: {
            auto m = decode_arrangedteam_invite_friend_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamMemberDecline: {
            auto m = decode_arrangedteam_member_decline(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamSendInvite: {
            auto m = decode_arrangedteam_send_invite(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidStartGame4: {
            auto m = decode_startgame4_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLadderList: {
            auto m = decode_ladder_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCheckAd: {
            auto m = decode_ad_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAdClick2: {
            auto m = decode_ad_click2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMotd: {
            auto m = decode_motd_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidChannelList: {
            auto m = decode_channel_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRegSnoop: {
            auto m = decode_regsnoop_request(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidProfile: {
            auto m = decode_profile_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidSetEmail: {
            auto m = decode_setemail_request(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidIconReq: {
            auto m = decode_icon_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCharList: {
            auto m = decode_char_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidServerList: {
            auto m = decode_server_list(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMessageBox: {
            auto m = decode_message_box(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRealmList: {
            auto m = decode_realm_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRealmJoin: {
            auto m = decode_realm_join_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidWarcraftGeneral: {
            auto m = decode_warcraft_general_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRequiredWork: {
            auto m = decode_required_work(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRealmListLegacy: {
            auto m = decode_realm_list_legacy_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCdKey3: {
            auto m = decode_cdkey3_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCreateAccount2: {
            auto m = decode_createaccount2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLoginW3: {
            auto m = decode_loginw3_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLogonProofW3: {
            auto m = decode_logonproof_w3_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidPassChange: {
            auto m = decode_passchange_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidPassChangeProof: {
            auto m = decode_passchange_proof_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCompInfo1: {
            auto m = decode_compreply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidProgIdent: {
            auto m = decode_authreq1_server(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAuthReq1: {
            auto m = decode_authreply1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidSessionKey1: {
            auto m = decode_sessionkey1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidSessionKey2: {
            auto m = decode_sessionkey2(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLogonResponse: {
            auto m = decode_loginreply1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCreateAccount1: {
            auto m = decode_createaccount1_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCdKeyLegacy: {
            auto m = decode_cdkey_legacy_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidChangePassword: {
            auto m = decode_changepassword_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCreateAccount: {
            auto m = decode_createaccount_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidStartGame1: {
            auto m = decode_startgame1_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidStartGame3: {
            auto m = decode_startgame3_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidReadMemory: {
            auto m = decode_read_memory_request(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMapAuth1: {
            auto m = decode_mapauthreply1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMapAuth2: {
            auto m = decode_mapauthreply2(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        default:
            return unimplemented(pkt.header.code);
    }
}

// --- encode -------------------------------------------------------------

core::Status<> encode(Writer& w, const Null&) {
    w.begin_bnet_packet(kSidNull);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const Ping& m) {
    w.begin_bnet_packet(kSidPing);
    w.write_le<std::uint32_t>(m.ticks);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AuthInfo& m) {
    w.begin_bnet_packet(kSidAuthInfo);
    w.write_le<std::uint32_t>(m.protocol_id);
    w.write_le<std::uint32_t>(m.platform_id);
    w.write_le<std::uint32_t>(m.game_id);
    w.write_le<std::uint32_t>(m.version_id);
    w.write_le<std::uint32_t>(m.language_id);
    w.write_le<std::uint32_t>(m.local_ip);
    w.write_le<std::uint32_t>(m.tz_bias);
    w.write_le<std::uint32_t>(m.mpq_locale);
    w.write_le<std::uint32_t>(m.lang_id);
    w.write_cstring(m.country_abbr);
    w.write_cstring(m.country);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AuthCheckReply& m) {
    w.begin_bnet_packet(kSidAuthCheck);
    w.write_le<std::uint32_t>(m.result);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AuthInfoReply& m) {
    w.begin_bnet_packet(kSidAuthInfo);
    w.write_le<std::uint32_t>(m.logontype);
    w.write_le<std::uint32_t>(m.server_token);
    w.write_le<std::uint32_t>(m.session_num);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.timestamp & 0xFFFFFFFFu));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.timestamp >> 32));
    w.write_cstring(m.mpq_filename);
    w.write_cstring(m.checksum_formula);
    if (!m.server_signature.empty()) {
        w.write_bytes(core::ByteView{
            reinterpret_cast<const std::byte*>(m.server_signature.data()),
            m.server_signature.size()});
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonResponse2& m) {
    w.begin_bnet_packet(kSidLogonResponse2);
    w.write_le<std::uint32_t>(m.client_token);
    w.write_le<std::uint32_t>(m.server_token);
    for (auto word : m.password_hash) w.write_le<std::uint32_t>(word);
    w.write_cstring(m.username);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonResponse2Reply& m) {
    w.begin_bnet_packet(kSidLogonResponse2);
    w.write_le<std::uint32_t>(m.result);
    // Only emit reason when present; mirrors legacy behaviour.
    if (!m.reason.empty() || m.result == 0x06u) {
        w.write_cstring(m.reason);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const JoinChannel& m) {
    w.begin_bnet_packet(kSidJoinChannel);
    w.write_le<std::uint32_t>(m.flags);
    w.write_cstring(m.channel);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const EnterChatRequest& m) {
    w.begin_bnet_packet(kSidEnterChat);
    w.write_cstring(m.username);
    w.write_cstring(m.statstring);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const EnterChatReply& m) {
    w.begin_bnet_packet(kSidEnterChat);
    w.write_cstring(m.unique_name);
    w.write_cstring(m.statstring);
    w.write_cstring(m.account);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChatCommand& m) {
    w.begin_bnet_packet(kSidChatCommand);
    w.write_cstring(m.text);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChatEvent& m) {
    w.begin_bnet_packet(kSidChatEvent);
    w.write_le<std::uint32_t>(m.event_id);
    w.write_le<std::uint32_t>(m.flags);
    w.write_le<std::uint32_t>(m.ping_ms);
    w.write_le<std::uint32_t>(m.user_ip);
    w.write_le<std::uint32_t>(m.acct_number);
    w.write_le<std::uint32_t>(m.registration);
    w.write_cstring(m.username);
    w.write_cstring(m.text);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AuthCheckRequest& m) {
    if (m.cdkeys.size() > 8u) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "bnet codec: AUTH_CHECK cdkeys > 8"));
    }
    w.begin_bnet_packet(kSidAuthCheck);
    w.write_le<std::uint32_t>(m.ticks);
    w.write_le<std::uint32_t>(m.gameversion);
    w.write_le<std::uint32_t>(m.checksum);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.cdkeys.size()));
    w.write_le<std::uint32_t>(m.spawn);
    for (const auto& key : m.cdkeys) {
        w.write_le<std::uint32_t>(key.public_value);
        w.write_le<std::uint32_t>(key.product);
        w.write_le<std::uint32_t>(key.checksum);
        w.write_le<std::uint32_t>(key.unknown);
        for (auto word : key.hash) w.write_le<std::uint32_t>(word);
    }
    w.write_cstring(m.exe_info);
    w.write_cstring(m.cdkey_owner);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const GameListRequest& m) {
    w.begin_bnet_packet(kSidGetAdvListEx);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint32_t>(m.max_games);
    w.write_cstring(m.game_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const GameListReply& m) {
    if (m.entries.size() > 1024u) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "bnet codec: GAMELISTREPLY entries > 1024"));
    }
    w.begin_bnet_packet(kSidGetAdvListEx);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.entries.size()));
    w.write_le<std::uint32_t>(m.sstatus);
    for (const auto& e : m.entries) {
        w.write_le<std::uint16_t>(e.gametype);
        w.write_le<std::uint16_t>(e.unknown1);
        w.write_le<std::uint16_t>(e.unknown3);
        w.write_be<std::uint16_t>(e.port);
        w.write_be<std::uint32_t>(e.game_ip);
        w.write_le<std::uint32_t>(e.unknown4);
        w.write_le<std::uint32_t>(e.unknown5);
        w.write_le<std::uint32_t>(e.status);
        w.write_le<std::uint32_t>(e.unknown6);
        w.write_cstring(e.game_name);
        w.write_cstring(e.password);
        w.write_cstring(e.info);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LadderSearchRequest& m) {
    w.begin_bnet_packet(kSidLadderSearch);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.type);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LadderSearchReply& m) {
    w.begin_bnet_packet(kSidLadderSearch);
    w.write_le<std::uint32_t>(m.rank);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FileInfoRequest& m) {
    w.begin_bnet_packet(kSidGetFileTime);
    w.write_le<std::uint32_t>(m.type);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_cstring(m.filename);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const FileInfoReply& m) {
    w.begin_bnet_packet(kSidGetFileTime);
    w.write_le<std::uint32_t>(m.type);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint64_t>(m.timestamp);
    w.write_cstring(m.filename);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CdKey2Request& m) {
    w.begin_bnet_packet(kSidCdKey2);
    w.write_le<std::uint32_t>(m.spawn);
    w.write_le<std::uint32_t>(m.keylen);
    w.write_le<std::uint32_t>(m.product_id);
    w.write_le<std::uint32_t>(m.key_value);
    w.write_le<std::uint32_t>(m.server_token);
    w.write_le<std::uint32_t>(m.ticks);
    for (auto word : m.key_hash) w.write_le<std::uint32_t>(word);
    w.write_cstring(m.owner);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CdKey2Reply& m) {
    w.begin_bnet_packet(kSidCdKey2);
    w.write_le<std::uint32_t>(m.result);
    if (!m.owner.empty()) w.write_cstring(m.owner);
    return w.finalize_bnet_packet();
}

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

core::Status<> encode(Writer& w, const StartGame4Request& m) {
    w.begin_bnet_packet(kSidStartGame4);
    w.write_le<std::uint16_t>(m.status);
    w.write_le<std::uint16_t>(m.flag);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.option);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const StartGame4Ack& m) {
    w.begin_bnet_packet(kSidStartGame4);
    w.write_le<std::uint32_t>(m.reply);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const UdpOk& m) {
    w.begin_bnet_packet(kSidUdpOk);
    w.write_le<std::uint32_t>(m.echo);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LadderListRequest& m) {
    w.begin_bnet_packet(kSidLadderList);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.type);
    w.write_le<std::uint32_t>(m.start);
    w.write_le<std::uint32_t>(m.count);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LadderListReply& m) {
    if (m.count > kLadderListLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: LADDERREPLY count exceeds limit"});
    }
    if (m.entries.size() != m.count) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: LADDERREPLY entries size mismatches count"});
    }
    w.begin_bnet_packet(kSidLadderList);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.type);
    w.write_le<std::uint32_t>(m.start);
    w.write_le<std::uint32_t>(m.count);
    for (const auto& e : m.entries) {
        w.write_le<std::uint32_t>(e.current.wins);
        w.write_le<std::uint32_t>(e.current.loss);
        w.write_le<std::uint32_t>(e.current.disconnect);
        w.write_le<std::uint32_t>(e.current.rating);
        w.write_le<std::uint32_t>(e.current.rank);
        w.write_le<std::uint32_t>(e.active.wins);
        w.write_le<std::uint32_t>(e.active.loss);
        w.write_le<std::uint32_t>(e.active.disconnect);
        w.write_le<std::uint32_t>(e.active.rating);
        w.write_le<std::uint32_t>(e.active.rank);
        for (auto v : e.ttest) {
            w.write_le<std::uint32_t>(v);
        }
        w.write_le<std::uint64_t>(e.lastgame_current);
        w.write_le<std::uint64_t>(e.lastgame_active);
        w.write_cstring(e.player_name);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AdRequest& m) {
    w.begin_bnet_packet(kSidCheckAd);
    w.write_le<std::uint32_t>(m.arch_tag);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_le<std::uint32_t>(m.prev_adid);
    w.write_le<std::uint32_t>(m.ticks);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AdReply& m) {
    w.begin_bnet_packet(kSidCheckAd);
    w.write_le<std::uint32_t>(m.adid);
    w.write_le<std::uint32_t>(m.extension_tag);
    w.write_le<std::uint64_t>(m.timestamp);
    w.write_cstring(m.filename);
    w.write_cstring(m.link);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AdClick& m) {
    w.begin_bnet_packet(kSidAdClick);
    w.write_le<std::uint32_t>(m.adid);
    w.write_le<std::uint32_t>(m.unknown1);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AdAck& m) {
    w.begin_bnet_packet(kSidAdAck);
    w.write_le<std::uint32_t>(m.arch_tag);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_le<std::uint32_t>(m.adid);
    w.write_cstring(m.adfile);
    w.write_cstring(m.adlink);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AdClick2Request& m) {
    w.begin_bnet_packet(kSidAdClick2);
    w.write_le<std::uint32_t>(m.adid);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AdClick2Reply& m) {
    w.begin_bnet_packet(kSidAdClick2);
    w.write_le<std::uint32_t>(m.adid);
    w.write_cstring(m.link);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const MotdRequest& m) {
    w.begin_bnet_packet(kSidMotd);
    w.write_le<std::uint32_t>(m.last_news_time);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const MotdReply& m) {
    w.begin_bnet_packet(kSidMotd);
    w.write_le<std::uint8_t>(m.msg_type);
    w.write_le<std::uint32_t>(m.curr_time);
    w.write_le<std::uint32_t>(m.first_news_time);
    w.write_le<std::uint32_t>(m.timestamp);
    w.write_le<std::uint32_t>(m.timestamp2);
    w.write_cstring(m.text);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChannelListRequest& m) {
    w.begin_bnet_packet(kSidChannelList);
    w.write_le<std::uint32_t>(m.client_tag);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChannelListReply& m) {
    if (m.channels.size() > kChannelListLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: CHANNELLIST exceeds limit"});
    }
    w.begin_bnet_packet(kSidChannelList);
    for (const auto& name : m.channels) {
        if (name.empty()) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "bnet codec: CHANNELLIST entry must not be empty"});
        }
        w.write_cstring(name);
    }
    // Terminator: an empty cstring.
    w.write_cstring("");
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LeaveChannel&) {
    w.begin_bnet_packet(kSidLeaveChat);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RegSnoopRequest& m) {
    w.begin_bnet_packet(kSidRegSnoop);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.hkey);
    w.write_cstring(m.reg_key);
    w.write_cstring(m.value_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RegSnoopReply& m) {
    w.begin_bnet_packet(kSidRegSnoop);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_bytes(core::ByteView{m.value.data(), m.value.size()});
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ProfileRequest& m) {
    w.begin_bnet_packet(kSidProfile);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ProfileReply& m) {
    w.begin_bnet_packet(kSidProfile);
    w.write_le<std::uint32_t>(m.cookie);
    w.write_le<std::uint8_t>(m.fail);
    if (m.fail == 0) {
        w.write_cstring(m.description);
        w.write_cstring(m.location);
        w.write_le<std::uint32_t>(m.clan_tag);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const SetEmailRequest&) {
    w.begin_bnet_packet(kSidSetEmail);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const SetEmailReply& m) {
    w.begin_bnet_packet(kSidSetEmail);
    w.write_cstring(m.email);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const IconRequest&) {
    w.begin_bnet_packet(kSidIconReq);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const IconReply& m) {
    w.begin_bnet_packet(kSidIconReq);
    w.write_le<std::uint64_t>(m.timestamp);
    w.write_cstring(m.filename);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const GetPasswordRequest& m) {
    w.begin_bnet_packet(kSidGetPassword);
    w.write_cstring(m.account_name);
    w.write_cstring(m.email);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChangeEmailRequest& m) {
    w.begin_bnet_packet(kSidChangeEmail);
    w.write_cstring(m.account_name);
    w.write_cstring(m.old_email);
    w.write_cstring(m.new_email);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CrashDump& m) {
    w.begin_bnet_packet(kSidCrashDump);
    w.write_bytes(core::ByteView{m.data.data(), m.data.size()});
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CharListRequest& m) {
    w.begin_bnet_packet(kSidCharList);
    w.write_le<std::uint32_t>(m.open_count);
    w.write_bytes(core::ByteView{m.char_data.data(), m.char_data.size()});
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CharListReply& m) {
    w.begin_bnet_packet(kSidCharList);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.max_chars);
    w.write_le<std::uint32_t>(m.count);
    w.write_bytes(core::ByteView{m.char_data.data(), m.char_data.size()});
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ServerList& m) {
    w.begin_bnet_packet(kSidServerList);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_cstring(m.servers);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const MessageBox& m) {
    w.begin_bnet_packet(kSidMessageBox);
    w.write_le<std::uint32_t>(m.style);
    w.write_cstring(m.text);
    w.write_cstring(m.caption);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RealmListRequest&) {
    w.begin_bnet_packet(kSidRealmList);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RealmListReply& m) {
    if (m.entries.size() > kRealmListLimit) {
        return core::fail(core::Error{core::StatusCode::OutOfRange,
                                      "REALMLISTREPLY count exceeds limit"});
    }
    w.begin_bnet_packet(kSidRealmList);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.entries.size()));
    for (const auto& e : m.entries) {
        w.write_le<std::uint32_t>(e.unknown);
        w.write_cstring(e.name);
        w.write_cstring(e.description);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RealmJoinRequest& m) {
    w.begin_bnet_packet(kSidRealmJoin);
    w.write_le<std::uint32_t>(m.seqno);
    for (auto v : m.seqno_hash) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.realm_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RealmJoinReply& m) {
    w.begin_bnet_packet(kSidRealmJoin);
    w.write_le<std::uint32_t>(m.seqno);
    w.write_le<std::uint32_t>(m.u1);
    w.write_le<std::uint32_t>(m.bncs_addr1);
    w.write_le<std::uint32_t>(m.session_num);
    w.write_le<std::uint32_t>(m.addr);
    w.write_be<std::uint16_t>(m.port);
    w.write_le<std::uint16_t>(m.u3);
    w.write_le<std::uint32_t>(m.session_key);
    w.write_le<std::uint32_t>(m.u5);
    w.write_le<std::uint32_t>(m.u6);
    w.write_le<std::uint32_t>(m.client_tag);
    w.write_le<std::uint32_t>(m.version_id);
    w.write_le<std::uint32_t>(m.bncs_addr2);
    w.write_le<std::uint32_t>(m.u7);
    for (auto v : m.secret_hash) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.account_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const WarcraftGeneralRequest& m) {
    w.begin_bnet_packet(kSidWarcraftGeneral);
    w.write_le<std::uint8_t>(m.sub_option);
    w.write_bytes(core::ByteView{m.data.data(), m.data.size()});
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const WarcraftGeneralReply& m) {
    w.begin_bnet_packet(kSidWarcraftGeneral);
    w.write_le<std::uint8_t>(m.sub_option);
    w.write_bytes(core::ByteView{m.data.data(), m.data.size()});
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RequiredWork& m) {
    w.begin_bnet_packet(kSidRequiredWork);
    w.write_cstring(m.filename);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ExtraWork& m) {
    if (m.data.size() > kExtraWorkMaxLen) {
        return core::fail(core::Error{core::StatusCode::OutOfRange,
                                      "EXTRAWORK length exceeds limit"});
    }
    w.begin_bnet_packet(kSidExtraWork);
    w.write_le<std::uint16_t>(m.game_type);
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(m.data.size()));
    w.write_bytes(core::ByteView{m.data.data(), m.data.size()});
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RealmListLegacyRequest& m) {
    w.begin_bnet_packet(kSidRealmListLegacy);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown2);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const RealmListLegacyReply& m) {
    if (m.entries.size() > kRealmListLimit) {
        return core::fail(core::Error{core::StatusCode::OutOfRange,
                                      "REALMLISTREPLY (legacy) count exceeds limit"});
    }
    w.begin_bnet_packet(kSidRealmListLegacy);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.entries.size()));
    for (const auto& e : m.entries) {
        w.write_le<std::uint32_t>(e.unknown3);
        w.write_le<std::uint32_t>(e.unknown4);
        w.write_le<std::uint32_t>(e.unknown5);
        w.write_le<std::uint32_t>(e.unknown6);
        w.write_le<std::uint32_t>(e.unknown7);
        w.write_le<std::uint32_t>(e.unknown8);
        w.write_le<std::uint32_t>(e.unknown9);
        w.write_cstring(e.name);
        w.write_cstring(e.description);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CdKey3Request& m) {
    w.begin_bnet_packet(kSidCdKey3);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_le<std::uint32_t>(m.unknown6);
    w.write_le<std::uint32_t>(m.unknown7);
    for (auto v : m.key_hash) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.owner_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CdKey3Reply& m) {
    w.begin_bnet_packet(kSidCdKey3);
    w.write_le<std::uint32_t>(m.message);
    if (!m.owner_name.empty()) {
        w.write_cstring(m.owner_name);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CreateAccount2Request& m) {
    w.begin_bnet_packet(kSidCreateAccount2);
    for (auto b : m.salt)              w.write_le<std::uint8_t>(b);
    for (auto b : m.password_verifier) w.write_le<std::uint8_t>(b);
    w.write_cstring(m.account_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CreateAccount2Reply& m) {
    w.begin_bnet_packet(kSidCreateAccount2);
    w.write_le<std::uint32_t>(m.result);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LoginW3Request& m) {
    w.begin_bnet_packet(kSidLoginW3);
    for (auto b : m.client_public_key) w.write_le<std::uint8_t>(b);
    w.write_cstring(m.account_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LoginW3Reply& m) {
    w.begin_bnet_packet(kSidLoginW3);
    w.write_le<std::uint32_t>(m.message);
    for (auto b : m.salt)              w.write_le<std::uint8_t>(b);
    for (auto b : m.server_public_key) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonProofW3Request& m) {
    w.begin_bnet_packet(kSidLogonProofW3);
    for (auto b : m.client_password_proof) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonProofW3Reply& m) {
    w.begin_bnet_packet(kSidLogonProofW3);
    w.write_le<std::uint32_t>(m.response);
    for (auto b : m.server_password_proof) w.write_le<std::uint8_t>(b);
    if (!m.message.empty()) {
        w.write_cstring(m.message);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeRequest& m) {
    w.begin_bnet_packet(kSidPassChange);
    for (auto b : m.client_public_key) w.write_le<std::uint8_t>(b);
    w.write_cstring(m.account_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeReply& m) {
    w.begin_bnet_packet(kSidPassChange);
    w.write_le<std::uint32_t>(m.message);
    for (auto b : m.salt)              w.write_le<std::uint8_t>(b);
    for (auto b : m.server_public_key) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeProofRequest& m) {
    w.begin_bnet_packet(kSidPassChangeProof);
    for (auto b : m.client_password_proof) w.write_le<std::uint8_t>(b);
    for (auto b : m.salt)                  w.write_le<std::uint8_t>(b);
    for (auto b : m.password_verifier)     w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeProofReply& m) {
    w.begin_bnet_packet(kSidPassChangeProof);
    w.write_le<std::uint32_t>(m.response);
    for (auto b : m.server_password_proof) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

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

core::Status<> encode(Writer& w, const UserDataReadRequest& m) {
    w.begin_bnet_packet(kSidReadUserData);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.names.size()));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.keys.size()));
    w.write_le<std::uint32_t>(m.request_id);
    for (const auto& s : m.names) w.write_cstring(s);
    for (const auto& s : m.keys)  w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const UserDataReadReply& m) {
    w.begin_bnet_packet(kSidReadUserData);
    w.write_le<std::uint32_t>(m.name_count);
    w.write_le<std::uint32_t>(m.key_count);
    w.write_le<std::uint32_t>(m.request_id);
    for (const auto& s : m.values) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const UserDataWriteRequest& m) {
    w.begin_bnet_packet(kSidWriteUserData);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.names.size()));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.keys.size()));
    for (const auto& s : m.names)  w.write_cstring(s);
    for (const auto& s : m.keys)   w.write_cstring(s);
    for (const auto& s : m.values) w.write_cstring(s);
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

// =========================================================================
// Legacy / OLS / pre-NLS encoders ("implement all SIDs" pass).
// =========================================================================

core::Status<> encode(Writer& w, const CompInfo1Request& m) {
    w.begin_bnet_packet(kSidCompInfo1);
    w.write_le<std::uint32_t>(m.reg_version);
    w.write_le<std::uint32_t>(m.reg_auth);
    w.write_le<std::uint32_t>(m.client_id);
    w.write_le<std::uint32_t>(m.client_token);
    if (!m.host.empty() || !m.user.empty()) {
        w.write_cstring(m.host);
        w.write_cstring(m.user);
    }
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CompReply& m) {
    w.begin_bnet_packet(kSidCompInfo1);
    w.write_le<std::uint32_t>(m.reg_version);
    w.write_le<std::uint32_t>(m.reg_auth);
    w.write_le<std::uint32_t>(m.client_id);
    w.write_le<std::uint32_t>(m.client_token);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const ProgIdent& m) {
    w.begin_bnet_packet(kSidProgIdent);
    w.write_le<std::uint32_t>(m.archtag);
    w.write_le<std::uint32_t>(m.clienttag);
    w.write_le<std::uint32_t>(m.versionid);
    w.write_le<std::uint32_t>(m.unknown1);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const AuthReq1Server& m) {
    w.begin_bnet_packet(kSidProgIdent);
    w.write_le<std::uint64_t>(m.timestamp);
    w.write_cstring(m.filename);
    w.write_cstring(m.equation);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const AuthReq1& m) {
    w.begin_bnet_packet(kSidAuthReq1);
    w.write_le<std::uint32_t>(m.archtag);
    w.write_le<std::uint32_t>(m.clienttag);
    w.write_le<std::uint32_t>(m.versionid);
    w.write_le<std::uint32_t>(m.gameversion);
    w.write_le<std::uint32_t>(m.checksum);
    w.write_cstring(m.exeinfo);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const AuthReply1& m) {
    w.begin_bnet_packet(kSidAuthReq1);
    w.write_le<std::uint32_t>(m.message);
    // Legacy SERVER_AUTHREPLY1 on-wire layout: optional filename
    // (only when non-empty) followed by exactly two trailing
    // NUL-terminated empty strings. Parity is required for real
    // clients to advance past the auth reply.
    if (!m.filename.empty()) w.write_cstring(m.filename);
    w.write_cstring("");
    w.write_cstring("");
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CountryInfo1& m) {
    w.begin_bnet_packet(kSidCountryInfo1);
    w.write_le<std::uint64_t>(m.systemtime);
    w.write_le<std::uint64_t>(m.localtime);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.bias));
    w.write_le<std::uint32_t>(m.langid1);
    w.write_le<std::uint32_t>(m.langid2);
    w.write_le<std::uint32_t>(m.langid3);
    w.write_cstring(m.langstr);
    w.write_cstring(m.countrycode);
    w.write_cstring(m.countryabbrev);
    w.write_cstring(m.countryname);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const SessionKey1& m) {
    w.begin_bnet_packet(kSidSessionKey1);
    w.write_le<std::uint32_t>(m.sessionkey);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const SessionKey2& m) {
    w.begin_bnet_packet(kSidSessionKey2);
    w.write_le<std::uint32_t>(m.sessionnum);
    w.write_le<std::uint32_t>(m.sessionkey);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CompInfo2& m) {
    w.begin_bnet_packet(kSidCompInfo2);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.reg_version);
    w.write_le<std::uint32_t>(m.reg_auth);
    w.write_le<std::uint32_t>(m.client_id);
    w.write_le<std::uint32_t>(m.client_token);
    if (!m.host.empty() || !m.user.empty()) {
        w.write_cstring(m.host);
        w.write_cstring(m.user);
    }
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const LoginReq1& m) {
    w.begin_bnet_packet(kSidLogonResponse);
    w.write_le<std::uint32_t>(m.ticks);
    w.write_le<std::uint32_t>(m.sessionkey);
    for (auto v : m.password_hash2) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const LoginReply1& m) {
    w.begin_bnet_packet(kSidLogonResponse);
    w.write_le<std::uint32_t>(m.message);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CreateAccount1Request& m) {
    w.begin_bnet_packet(kSidCreateAccount1);
    for (auto v : m.password_hash1) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CreateAccount1Reply& m) {
    w.begin_bnet_packet(kSidCreateAccount1);
    w.write_le<std::uint32_t>(m.result);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const Unknown2B& m) {
    w.begin_bnet_packet(kSidUnknown2B);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_le<std::uint32_t>(m.unknown6);
    w.write_le<std::uint32_t>(m.unknown7);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CdKeyLegacyRequest& m) {
    w.begin_bnet_packet(kSidCdKeyLegacy);
    w.write_le<std::uint32_t>(m.spawn);
    w.write_cstring(m.cdkey);
    if (!m.owner_name.empty()) w.write_cstring(m.owner_name);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CdKeyLegacyReply& m) {
    w.begin_bnet_packet(kSidCdKeyLegacy);
    w.write_le<std::uint32_t>(m.message);
    if (!m.owner_name.empty()) w.write_cstring(m.owner_name);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const ChangePasswordRequest& m) {
    w.begin_bnet_packet(kSidChangePassword);
    w.write_le<std::uint32_t>(m.ticks);
    w.write_le<std::uint32_t>(m.sessionkey);
    for (auto v : m.oldpassword_hash2) w.write_le<std::uint32_t>(v);
    for (auto v : m.newpassword_hash1) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.player_name);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const ChangePasswordReply& m) {
    w.begin_bnet_packet(kSidChangePassword);
    w.write_le<std::uint32_t>(m.message);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const Unknown39& m) {
    w.begin_bnet_packet(kSidUnknown39);
    w.write_cstring(m.char_name);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CreateAccountRequest& m) {
    w.begin_bnet_packet(kSidCreateAccount);
    for (auto v : m.password_hash1) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.username);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CreateAccountReply& m) {
    w.begin_bnet_packet(kSidCreateAccount);
    w.write_le<std::uint32_t>(m.result);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const NetGamePort& m) {
    w.begin_bnet_packet(kSidNetGamePort);
    w.write_le<std::uint16_t>(m.port);
    return w.finalize_bnet_packet();
}

// --- Game-lifecycle encoders --------------------------------------------

core::Status<> encode(Writer& w, const CloseGame&) {
    w.begin_bnet_packet(kSidCloseGame);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const CloseGame2&) {
    w.begin_bnet_packet(kSidCloseGame2);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const StartGame1Request& m) {
    w.begin_bnet_packet(kSidStartGame1);
    w.write_le<std::uint32_t>(m.status);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const StartGame1Ack& m) {
    w.begin_bnet_packet(kSidStartGame1);
    w.write_le<std::uint32_t>(m.reply);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const StartGame3Request& m) {
    w.begin_bnet_packet(kSidStartGame3);
    w.write_le<std::uint32_t>(m.status);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown6);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const StartGame3Ack& m) {
    w.begin_bnet_packet(kSidStartGame3);
    w.write_le<std::uint32_t>(m.reply);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const JoinGame& m) {
    w.begin_bnet_packet(kSidJoinGame);
    w.write_le<std::uint32_t>(m.clienttag);
    w.write_le<std::uint32_t>(m.versiontag);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const GameReport& m) {
    if (m.results.size() != m.player_names.size()) {
        return core::fail(core::Error{core::StatusCode::InvalidArgument,
            "bnet codec: GAMEREPORT results/player_names size mismatch"});
    }
    w.begin_bnet_packet(kSidGameReport);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.results.size()));
    for (auto r : m.results) w.write_le<std::uint32_t>(r);
    for (const auto& n : m.player_names) w.write_cstring(n);
    w.write_cstring(m.report_header);
    w.write_cstring(m.report_body);
    return w.finalize_bnet_packet();
}

// --- Misc / anti-cheat / advisory encoders ------------------------------

core::Status<> encode(Writer& w, const ReadMemoryRequest& m) {
    w.begin_bnet_packet(kSidReadMemory);
    w.write_le<std::uint32_t>(m.request_id);
    w.write_le<std::uint32_t>(m.address);
    w.write_le<std::uint32_t>(m.length);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const ReadMemoryReply& m) {
    w.begin_bnet_packet(kSidReadMemory);
    w.write_le<std::uint32_t>(m.request_id);
    for (auto b : m.memory) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const Unknown1B& m) {
    w.begin_bnet_packet(kSidUnknown1B);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint16_t>(m.port_be);
    w.write_le<std::uint32_t>(m.ip_be);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint32_t>(m.unknown3);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const Unknown24&) {
    w.begin_bnet_packet(kSidUnknown24);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const MapAuthReq1& m) {
    w.begin_bnet_packet(kSidMapAuth1);
    for (auto v : m.file_checksum) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.mapfile);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const MapAuthReply1& m) {
    w.begin_bnet_packet(kSidMapAuth1);
    w.write_le<std::uint32_t>(m.response);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const MapAuthReq2& m) {
    w.begin_bnet_packet(kSidMapAuth2);
    w.write_le<std::uint32_t>(m.unknown);
    for (auto v : m.file_hash) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.mapfile);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const MapAuthReply2& m) {
    w.begin_bnet_packet(kSidMapAuth2);
    w.write_le<std::uint32_t>(m.response);
    return w.finalize_bnet_packet();
}
core::Status<> encode(Writer& w, const ChangeClient& m) {
    w.begin_bnet_packet(kSidChangeClient);
    w.write_le<std::uint32_t>(m.clienttag);
    return w.finalize_bnet_packet();
}

}  // namespace pvpgn::protocol::bnet
