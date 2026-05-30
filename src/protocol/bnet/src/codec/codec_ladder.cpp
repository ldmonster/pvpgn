// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

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


} // namespace detail

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


} // namespace pvpgn::protocol::bnet
