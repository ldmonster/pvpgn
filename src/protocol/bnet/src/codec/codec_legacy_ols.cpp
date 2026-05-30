// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

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


} // namespace detail

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
    w.write_cstring(m.owner_name);
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


} // namespace pvpgn::protocol::bnet
