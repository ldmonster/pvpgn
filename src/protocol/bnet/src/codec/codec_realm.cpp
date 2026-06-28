// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

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
    {
        // addr is network-order (big-endian), matching the encoder + the
        // original's bn_int_nset. Keep encode/decode symmetric.
        auto v = r.read_be<std::uint32_t>();
        if (!v) return core::fail(v.error());
        m.addr = v.value();
    }
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


} // namespace detail

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
    if (m.entries.size() > detail::kRealmListLimit) {
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
    // The realm (d2cs) address is a network-order IP, exactly like the port
    // below — the original writes both with bn_*_nset (big-endian). (Latent
    // until v3 grows a realm backend; addr is 0 today, but keep it correct and
    // consistent with port.)
    w.write_be<std::uint32_t>(m.addr);
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
    if (m.data.size() > detail::kExtraWorkMaxLen) {
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
    if (m.entries.size() > detail::kRealmListLimit) {
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


} // namespace pvpgn::protocol::bnet
