// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

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


} // namespace detail

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
    w.write_cstring(m.owner);
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
    w.write_cstring(m.owner_name);
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


} // namespace pvpgn::protocol::bnet
