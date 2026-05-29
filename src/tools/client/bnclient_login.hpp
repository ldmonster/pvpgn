// SPDX-License-Identifier: GPL-2.0-or-later
//
// BNet login / handshake driver for the v3 client tools.
// Header-only.
//
// This is a direct, modern port of what
// `src/v3/tools/client/client_connect.cpp` does: open a TCP
// connection to bnetd, send the init-class byte, drive the
// pre-login handshake (`CLIENT_AUTH_INFO` -> optional
// `SERVER_AUTHREQ_109` / `CLIENT_AUTHREQ_109` ->
// `SERVER_AUTHREPLY_109` -> `CLIENT_ICONREQ` /
// `SERVER_ICONREPLY` -> optional `CLIENT_CDKEY2` /
// `SERVER_CDKEYREPLY2`), and return the connected socket
// together with the negotiated `sessionkey` / `sessionnum` so
// the caller (`bnstat`, `bnchat`, ...) can move on to its own
// CLIENT_LOGINREQ1 step.
//
// The default version-info table (`default_version_info()`) is
// a cleanroom copy of the same constants in
// `client_connect.cpp::get_defversioninfo()`; it is NOT a load
// of `versioncheck.conf`.  For the v3 client tools that talks
// directly to bnetd, a static table is sufficient as long as
// `Config::ignoreversion` is `true` (the default).
//
// Network I/O is driven by the vendored `bnclient_net.hpp`
// helpers and packet framing by `bnclient_proto.hpp`.  No
// dependency on legacy `common/*`.

#ifndef PVPGN_V3_CLIENT_BNCLIENT_LOGIN_HPP
#define PVPGN_V3_CLIENT_BNCLIENT_LOGIN_HPP

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

#include "bnclient_bnet_packets.hpp"
#include "bnclient_hash.hpp"
#include "bnclient_net.hpp"
#include "bnclient_proto.hpp"

namespace pvpgn::client_v3::login {

// ---- default version info table --------------------------------------

struct VersionInfo {
    std::uint32_t versionid   = 0;
    std::uint32_t gameversion = 0;
    std::string   exeinfo;
    std::uint32_t checksum    = 0;
};

// Returns the canonical version info for a known 4cc clienttag.
// Mirrors `client_connect.cpp::get_defversioninfo()`.  Unknown
// tags yield an all-default `VersionInfo` so callers can still
// drive an `ignoreversion=true` handshake without erroring out.
inline VersionInfo default_version_info(std::string_view clienttag) {
    VersionInfo v;
    if (clienttag == "DRTL") {
        v.versionid   = 0x00000001;
        v.gameversion = 0x00010206;
        v.exeinfo     = "Diablo.exe 09/29/98 15:43:51 1187088";
        v.checksum    = 0xae07e0c8;
    } else if (clienttag == "STAR") {
        v.versionid   = 0x000000c3;
        v.gameversion = 0x01080001;
        v.exeinfo     = "starcraft.exe 12/28/00 11:28:52 1082368";
        v.checksum    = 0xa157ce21;
    } else if (clienttag == "SSHR") {
        v.versionid   = 0x000000a8;
        v.gameversion = 0x01080001;
        v.exeinfo     = "starcraft.exe 08/03/00 17:35:46 1015552";
        v.checksum    = 0xa157ce21;
    } else if (clienttag == "SEXP") {
        v.versionid   = 0x000000c3;
        v.gameversion = 0x01080001;
        v.exeinfo     = "starcraft.exe 12/28/00 11:28:52 1082368";
        v.checksum    = 0xa157ce21;
    } else if (clienttag == "W2BN") {
        v.versionid   = 0x0000004f;
        v.gameversion = 0x02000099;
        v.exeinfo     = "Warcraft II BNE.exe 11/05/99 17:24:14 707344";
        v.checksum    = 0xb7c12c1f;
    } else if (clienttag == "D2DV") {
        v.versionid   = 0x0000000d;
        v.gameversion = 0x01000900;
        v.exeinfo     = "Game.exe 08/16/01 23:04:40 424067";
        v.checksum    = 0xf762cd46;
    } else if (clienttag == "D2XP") {
        v.versionid   = 0x0000000a;
        v.gameversion = 0x01000900;
        v.exeinfo     = "Game.exe 08/16/01 23:04:42 425779";
        v.checksum    = 0xf762cd46;
    } else if (clienttag == "WAR3" || clienttag == "W3XP") {
        v.versionid   = 0x00000001;
        v.gameversion = 0x01000600;
        v.exeinfo     = "War3.exe 04/14/03 15:43:23 471040";
        v.checksum    = 0x6020df51;
    }
    // CHAT / unknown -> all zero, fine when ignoreversion=true.
    return v;
}

// ---- session driver --------------------------------------------------

struct Config {
    std::string   server       = "localhost";
    std::uint16_t port         = 6112;
    // Clienttag/archtag/gamelang are 4-character strings (NOT
    // NUL-terminated).  These match the legacy CLIENTTAG_* / "IX86" /
    // "enUS" literals; pass them with .substr(0,4) if uncertain.
    std::string   clienttag    = "CHAT";
    std::string   archtag      = "IX86";
    std::string   gamelang     = "enUS";
    std::string   cdowner      = "owner";
    std::string   cdkey;            // empty -> all-zero key material
    bool          ignoreversion = true;
    // STAR/SEXP/W2BN normally require a CDKEY2 step; set to true
    // for those tags.  Other tags should leave it false.
    bool          send_cdkey2  = false;
};

struct Result {
    net::socket_t sock        = net::kInvalidSocket;
    std::uint32_t sessionkey  = 0;
    std::uint32_t sessionnum  = 0;
};

class Session {
public:
    explicit Session(Config cfg) : cfg_(std::move(cfg)) {}

    // Runs the full pre-login handshake.  On success returns true
    // and `out.sock` is a connected socket positioned just after
    // `SERVER_ICONREPLY` (and `SERVER_CDKEYREPLY2` if requested).
    // On failure returns false; `error()` describes the cause and
    // any socket that was opened has been closed.
    bool run(Result& out) {
        // Make sure platform sockets are initialised.  Idempotent
        // and cheap on POSIX; required on Windows.
        net::sockets_startup();

        out = Result{};

        net::socket_holder sock{net::connect_tcp(cfg_.server, cfg_.port)};
        if (!sock.valid()) {
            return fail("connect to " + cfg_.server + " failed");
        }
        const net::socket_t sd = sock.get();

        if (!proto::send_init_classbyte(sd, proto::init_class::Bnet)) {
            return fail("send init classbyte failed");
        }

        // Diablo classic shareware/retail expects an extra
        // UNKNOWN_1B before AUTH_INFO.  Send zeros for ip/port.
        if (cfg_.clienttag == "DRTL" || cfg_.clienttag == "DSHR") {
            if (!send_unknown_1b(sd)) {
                return fail("send UNKNOWN_1B failed");
            }
        }

        if (!send_auth_info(sd)) {
            return fail("send AUTH_INFO failed");
        }

        // Drain server packets until AUTHREQ_109 or AUTHREPLY_109.
        proto::Packet rpkt;
        for (;;) {
            if (!proto::recv_bnet(sd, rpkt)) {
                return fail("server closed before AUTH challenge");
            }
            const auto t = rpkt.bnet_type();
            if (t == bnet::packet_id::SERVER_AUTHREQ_109 ||
                t == bnet::packet_id::SERVER_AUTHREPLY_109) {
                break;
            }
            // Ignore unrelated server-pushed packets (e.g. PING).
        }

        if (rpkt.bnet_type() == bnet::packet_id::SERVER_AUTHREQ_109) {
            // Capture sessionkey/sessionnum and (optionally) run
            // the version-check round.
            const auto* req = rpkt.body_as<bnet::SServerAuthReq109>();
            out.sessionkey  = proto::int_get(req->sessionkey);
            out.sessionnum  = proto::int_get(req->sessionnum);

            if (!cfg_.ignoreversion) {
                if (!send_authreq_109(sd)) {
                    return fail("send CLIENT_AUTHREQ_109 failed");
                }
                for (;;) {
                    if (!proto::recv_bnet(sd, rpkt)) {
                        return fail("server closed before AUTHREPLY_109");
                    }
                    if (rpkt.bnet_type() == bnet::packet_id::SERVER_AUTHREPLY_109) {
                        break;
                    }
                }
                const auto* rep = rpkt.body_as<bnet::SServerAuthReply109>();
                if (proto::int_get(rep->message) != bnet::kAuthReply109_Ok) {
                    return fail("AUTHREPLY_109 rejected");
                }
            }
        }
        // Else: legacy server skipped the challenge; sessionkey
        // stays 0.  Caller has to handle that path itself.

        if (!send_iconreq(sd)) {
            return fail("send ICONREQ failed");
        }
        for (;;) {
            if (!proto::recv_bnet(sd, rpkt)) {
                return fail("server closed before ICONREPLY");
            }
            if (rpkt.bnet_type() == bnet::packet_id::SERVER_ICONREPLY) {
                break;
            }
        }

        if (cfg_.send_cdkey2) {
            if (!send_cdkey2(sd, out.sessionkey)) {
                return fail("send CDKEY2 failed");
            }
            for (;;) {
                if (!proto::recv_bnet(sd, rpkt)) {
                    return fail("server closed before CDKEYREPLY2");
                }
                if (rpkt.bnet_type() == bnet::packet_id::SERVER_CDKEYREPLY2) {
                    break;
                }
            }
            // Don't fail on non-OK -- legacy bnstat ignored it too.
        }

        out.sock = sock.release();
        return true;
    }

    const std::string& error() const noexcept { return error_; }

private:
    Config      cfg_;
    std::string error_;

    bool fail(std::string msg) {
        error_ = std::move(msg);
        return false;
    }

    // Copy a 4-character tag into a `char[4]` for `int_tag_set`.
    // If `s` is shorter, pad with NULs (matches legacy behaviour).
    static void pack_tag(const std::string& s, char out[4]) noexcept {
        for (std::size_t i = 0; i < 4; ++i) {
            out[i] = (i < s.size()) ? s[i] : '\0';
        }
    }

    bool send_unknown_1b(net::socket_t sd) {
        proto::Packet pkt;
        pkt.set_bnet_type(bnet::packet_id::CLIENT_UNKNOWN_1B);
        pkt.set_bnet_size(static_cast<std::uint16_t>(
            proto::kBnetHeaderSize + sizeof(bnet::CClientUnknown1B)));
        auto* u = pkt.body_as<bnet::CClientUnknown1B>();
        proto::short_set(u->unknown1, bnet::kUnknown1B_Unknown1);
        // port and ip are big-endian on the wire; we always send 0
        // so byte order is irrelevant -- zero-fill explicitly.
        u->port = proto::bn_short{};
        proto::int_nset (u->ip,       0);
        proto::int_set  (u->unknown2, bnet::kUnknown1B_Unknown1);
        proto::int_set  (u->unknown3, bnet::kUnknown1B_Unknown1);
        return proto::send_bnet(sd, pkt);
    }

    bool send_auth_info(net::socket_t sd) {
        const VersionInfo vi = default_version_info(cfg_.clienttag);

        proto::Packet pkt;
        pkt.set_bnet_type(bnet::packet_id::CLIENT_AUTH_INFO);
        pkt.set_bnet_size(static_cast<std::uint16_t>(
            proto::kBnetHeaderSize + sizeof(bnet::CClientAuthInfo)));
        auto* ai = pkt.body_as<bnet::CClientAuthInfo>();

        char arch[4]{};  pack_tag(cfg_.archtag,   arch);
        char ctag[4]{};  pack_tag(cfg_.clienttag, ctag);
        char glng[4]{};  pack_tag(cfg_.gamelang,  glng);

        proto::int_set    (ai->protocol,  0);
        proto::int_tag_set(ai->archtag,   arch);
        proto::int_tag_set(ai->clienttag, ctag);
        proto::int_set    (ai->versionid, vi.versionid);
        proto::int_tag_set(ai->gamelang,  glng);
        proto::int_set    (ai->localip,   0);
        proto::int_set    (ai->bias,      0);
        proto::int_set    (ai->lcid,      bnet::kAuthInfoLangIdUsEnglish);
        proto::int_set    (ai->langid,    bnet::kAuthInfoLangIdUsEnglish);

        if (!pkt.append_cstr(bnet::kAuthInfoLangStrUsEnglish)) return false;
        if (!pkt.append_cstr(bnet::kAuthInfoCountryUsa))       return false;
        pkt.set_bnet_size(static_cast<std::uint16_t>(pkt.size()));
        return proto::send_bnet(sd, pkt);
    }

    bool send_authreq_109(net::socket_t sd) {
        const VersionInfo vi = default_version_info(cfg_.clienttag);

        proto::Packet pkt;
        pkt.set_bnet_type(bnet::packet_id::CLIENT_AUTHREQ_109);
        pkt.set_bnet_size(static_cast<std::uint16_t>(
            proto::kBnetHeaderSize + sizeof(bnet::CClientAuthReq109)));
        auto* ar = pkt.body_as<bnet::CClientAuthReq109>();
        proto::int_set(ar->ticks,        static_cast<std::uint32_t>(std::time(nullptr)));
        proto::int_set(ar->gameversion,  vi.gameversion);
        proto::int_set(ar->checksum,     vi.checksum);
        proto::int_set(ar->cdkey_number, 1);
        proto::int_set(ar->spawn,        0);

        // One empty cdkey block.
        bnet::CdkeyInfo info{};
        if (!pkt.append(&info, sizeof(info))) return false;
        if (!pkt.append_cstr(vi.exeinfo.c_str())) return false;
        if (!pkt.append_cstr(cfg_.cdowner.c_str())) return false;
        pkt.set_bnet_size(static_cast<std::uint16_t>(pkt.size()));
        return proto::send_bnet(sd, pkt);
    }

    bool send_iconreq(net::socket_t sd) {
        proto::Packet pkt;
        pkt.set_bnet_type(bnet::packet_id::CLIENT_ICONREQ);
        pkt.set_bnet_size(static_cast<std::uint16_t>(proto::kBnetHeaderSize));
        return proto::send_bnet(sd, pkt);
    }

    bool send_cdkey2(net::socket_t sd, std::uint32_t sessionkey) {
        // Cleartext blob hashed with the broken-SHA1 variant from
        // `bnclient_hash.hpp`.
        struct {
            proto::bn_int sessionkey;
            proto::bn_int ticks;
            proto::bn_int productid;
            proto::bn_int keyvalue1;
            proto::bn_int keyvalue2;
        } seed{};

        // Without real key-derivation we send all-zero key material;
        // the server will reject it but legacy bnstat tolerates this.
        const std::uint32_t ticks     = 0;
        const std::uint32_t productid = 0;
        const std::uint32_t keyvalue1 = 0;
        const std::uint32_t keyvalue2 = 0;
        proto::int_set(seed.sessionkey, sessionkey);
        proto::int_set(seed.ticks,      ticks);
        proto::int_set(seed.productid,  productid);
        proto::int_set(seed.keyvalue1,  keyvalue1);
        proto::int_set(seed.keyvalue2,  keyvalue2);
        const hash::HashDigest digest = hash::bnet_hash(&seed, sizeof(seed));

        proto::Packet pkt;
        pkt.set_bnet_type(bnet::packet_id::CLIENT_CDKEY2);
        pkt.set_bnet_size(static_cast<std::uint16_t>(
            proto::kBnetHeaderSize + sizeof(bnet::CClientCdkey2)));
        auto* c = pkt.body_as<bnet::CClientCdkey2>();
        proto::int_set(c->spawn,      bnet::kCdkey2_Spawn_False);
        proto::int_set(c->keylen,
            static_cast<std::uint32_t>(cfg_.cdkey.size()));
        proto::int_set(c->productid,  productid);
        proto::int_set(c->keyvalue1,  keyvalue1);
        proto::int_set(c->sessionkey, sessionkey);
        proto::int_set(c->ticks,      ticks);
        for (std::size_t i = 0; i < 5; ++i) {
            proto::int_set(c->key_hash[i], digest[i]);
        }
        if (!pkt.append_cstr(cfg_.cdowner.c_str())) return false;
        pkt.set_bnet_size(static_cast<std::uint16_t>(pkt.size()));
        return proto::send_bnet(sd, pkt);
    }
};

} // namespace pvpgn::client_v3::login

#endif // PVPGN_V3_CLIENT_BNCLIENT_LOGIN_HPP
