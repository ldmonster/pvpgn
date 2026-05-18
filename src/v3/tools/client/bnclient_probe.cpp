// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compile-time smoke probe for the v3 client vendored headers.  This
// translation unit exists only so the build catches header
// regressions even before any tool is rewritten to consume them.  It
// produces no runtime code; the tiny exported anchor keeps the
// object from being dropped by the linker.

#include "bnclient_net.hpp"
#include "bnclient_proto.hpp"
#include "bnclient_hash.hpp"
#include "bnclient_bnet_packets.hpp"
#include "bnclient_login.hpp"

namespace pvpgn::client_v3 {

// Touch every key API surface so unused-function warnings would
// surface real bugs.
namespace {

[[maybe_unused]] void compile_probe() noexcept {
    using namespace proto;

    Packet p;
    p.clear();
    p.set_bnet_type(0);
    p.set_bnet_size(static_cast<std::uint16_t>(kBnetHeaderSize));
    (void)p.bnet_type();
    (void)p.bnet_size();
    (void)p.data();
    (void)p.size();
    p.append("x", 1);

    bn_byte  bb{};
    bn_short bs{};
    bn_int   bi{};
    byte_set(bb, 1);
    short_set(bs, 2);
    int_set(bi, 3);
    (void)byte_get(bb);
    (void)short_get(bs);
    (void)int_get(bi);
    (void)short_nget(bs);
    (void)int_nget(bi);
    int_nset(bi, 7);
    int_tag_set(bi, "IX86");
    char tag[5];
    int_tag_get(bi, tag);

    // Hash surface.  Two identical inputs must produce equal digests
    // and a hash of an empty buffer must be deterministic.
    using namespace hash;
    HashDigest h1 = bnet_hash("password", 8);
    HashDigest h2 = bnet_hash("password", 8);
    HashDigest h3 = bnet_hash(nullptr, 0);
    (void)eq(h1, h2);
    (void)eq(h1, h3);
    (void)to_string(h1);

    struct PodSample { std::uint32_t a; std::uint32_t b; };
    PodSample sample{0x12345678u, 0x9abcdef0u};
    HashDigest h4 = bnet_hash_object(sample);
    (void)h4;

    // BNet packet bodies: instantiate, fill via vendored setters,
    // and round-trip through Packet::body_as<T>().  Static asserts
    // in the header already pin the wire sizes; here we exercise
    // the field setters so any typo surfaces under -Werror.
    using namespace bnet;
    {
        Packet pkt;
        pkt.set_bnet_type(packet_id::CLIENT_AUTH_INFO);
        pkt.set_bnet_size(static_cast<std::uint16_t>(
            kBnetHeaderSize + sizeof(CClientAuthInfo)));
        auto* ai = pkt.body_as<CClientAuthInfo>();
        int_set     (ai->protocol, 0);
        int_tag_set (ai->archtag,   tag::archtag_IX86);
        int_tag_set (ai->clienttag, tag::clienttag_STAR);
        int_set     (ai->versionid, 1);
        int_tag_set (ai->gamelang,  tag::gamelang_enUS);
        int_set     (ai->localip,   0);
        int_set     (ai->bias,      0);
        int_set     (ai->lcid,      kAuthInfoLangIdUsEnglish);
        int_set     (ai->langid,    kAuthInfoLangIdUsEnglish);
        (void)pkt.append_cstr(kAuthInfoLangStrUsEnglish);
        (void)pkt.append_cstr(kAuthInfoCountryUsa);
    }
    {
        Packet pkt;
        pkt.set_bnet_type(packet_id::CLIENT_LOGINREQ1);
        pkt.set_bnet_size(static_cast<std::uint16_t>(
            kBnetHeaderSize + sizeof(CClientLoginReq1)));
        auto* lr = pkt.body_as<CClientLoginReq1>();
        int_set(lr->ticks, 0);
        int_set(lr->sessionkey, 0);
        for (auto& w : lr->password_hash2) {
            int_set(w, 0);
        }
        (void)pkt.append_cstr("player");
    }
    {
        SServerAuthReply109 reply{};
        int_set(reply.message, kAuthReply109_Ok);
        (void)reply;
        SServerLoginReply1 lreply{};
        int_set(lreply.message, kLoginReply1_Success);
        (void)lreply;
    }
    {
        Packet pkt;
        pkt.set_bnet_type(packet_id::CLIENT_STATSREQ);
        pkt.set_bnet_size(static_cast<std::uint16_t>(
            kBnetHeaderSize + sizeof(CClientStatsReq)));
        auto* sr = pkt.body_as<CClientStatsReq>();
        int_set(sr->name_count, 1);
        int_set(sr->key_count,  0);
        int_set(sr->requestid,  kStatsReqRequestId);
        (void)pkt.append_cstr("player");
    }

    // Login session driver: keep the public surface alive (no
    // runtime I/O -- a successful build is the smoke check).
    {
        using namespace login;
        Config cfg;
        cfg.server        = "127.0.0.1";
        cfg.port          = 6112;
        cfg.clienttag     = "STAR";
        cfg.archtag       = "IX86";
        cfg.gamelang      = "enUS";
        cfg.cdowner       = "anon";
        cfg.ignoreversion = true;
        cfg.send_cdkey2   = true;
        VersionInfo vi = default_version_info(cfg.clienttag);
        (void)vi.versionid;
        (void)vi.gameversion;
        (void)vi.exeinfo.size();
        (void)vi.checksum;
        Session ses{cfg};
        (void)ses.error();
        auto run_ptr = &Session::run;
        (void)run_ptr;
        Result res{};
        (void)res.sock;
    }
}

} // namespace

int bnclient_v3_probe_anchor() noexcept { return 0; }

} // namespace pvpgn::client_v3
