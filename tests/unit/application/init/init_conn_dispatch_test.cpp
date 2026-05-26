// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn::application::init::dispatch_init_conn`.
// Pins the byte-to-decision table against the legacy
// `CLIENT_INITCONN_CLASS_*` constants so any drift between
// `protocol/bnet/init_wire_types.hpp` and the dispatcher is caught
// here.

#include <catch2/catch_test_macros.hpp>

#include "application/init/init_conn_dispatch.hpp"
#include "protocol/bnet/init_wire_types.hpp"

namespace appinit = pvpgn::application::init;
namespace bniw    = pvpgn::protocol::bnet::init;

TEST_CASE("dispatch_init_conn accepts CLIENT_INITCONN_CLASS_BNET",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({bniw::kClassBnet});
    REQUIRE(r.decision == appinit::InitDecision::kBnet);
}

TEST_CASE("dispatch_init_conn accepts CLIENT_INITCONN_CLASS_FILE",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({bniw::kClassFile});
    REQUIRE(r.decision == appinit::InitDecision::kFile);
}

TEST_CASE("dispatch_init_conn accepts CLIENT_INITCONN_CLASS_BOT",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({bniw::kClassBot});
    REQUIRE(r.decision == appinit::InitDecision::kBot);
}

TEST_CASE("dispatch_init_conn accepts CLIENT_INITCONN_CLASS_TELNET",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({bniw::kClassTelnet});
    REQUIRE(r.decision == appinit::InitDecision::kTelnet);
}

TEST_CASE("dispatch_init_conn accepts CLIENT_INITCONN_CLASS_D2CS_BNETD",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({bniw::kClassD2csBnetd});
    REQUIRE(r.decision == appinit::InitDecision::kD2csBnetd);
}

TEST_CASE("dispatch_init_conn rejects CLIENT_INITCONN_CLASS_ENC",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({bniw::kClassEnc});
    REQUIRE(r.decision == appinit::InitDecision::kRejected);
}

TEST_CASE("dispatch_init_conn rejects CLIENT_INITCONN_CLASS_LOCALMACHINE",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({bniw::kClassLocalMachine});
    REQUIRE(r.decision == appinit::InitDecision::kRejected);
}

TEST_CASE("dispatch_init_conn rejects D2GS (0x64) at bnetd",
          "[application][init][dispatch]") {
    // D2GS clients go to the d2cs listener, never to bnetd. From
    // bnetd's perspective they must be rejected.
    const auto r = appinit::dispatch_init_conn({bniw::kClassD2gs});
    REQUIRE(r.decision == appinit::InitDecision::kRejected);
}

TEST_CASE("dispatch_init_conn rejects zero byte",
          "[application][init][dispatch]") {
    const auto r = appinit::dispatch_init_conn({0x00});
    REQUIRE(r.decision == appinit::InitDecision::kRejected);
}

TEST_CASE("dispatch_init_conn rejects every other byte",
          "[application][init][dispatch]") {
    int accepted = 0;
    int rejected = 0;
    for (int b = 0; b <= 0xff; ++b) {
        const auto r = appinit::dispatch_init_conn(
            {static_cast<std::uint8_t>(b)});
        if (r.decision == appinit::InitDecision::kRejected) {
            ++rejected;
        } else {
            ++accepted;
        }
    }
    // Exactly five distinct bytes are accepted today (0x01 Bnet,
    // 0x02 File, 0x03 Bot, 0x0d Telnet, 0x65 D2csBnetd). If this
    // count changes, update both the dispatcher table and the
    // wire-types constants together.
    REQUIRE(accepted == 5);
    REQUIRE(rejected == 251);
}

TEST_CASE("InitConnRequest equality is value-based",
          "[application][init][dispatch]") {
    REQUIRE(appinit::InitConnRequest{0x01}
            == appinit::InitConnRequest{0x01});
    REQUIRE_FALSE(appinit::InitConnRequest{0x01}
                  == appinit::InitConnRequest{0x02});
}

TEST_CASE("InitConnResponse equality is value-based",
          "[application][init][dispatch]") {
    REQUIRE(appinit::InitConnResponse{appinit::InitDecision::kBnet}
            == appinit::InitConnResponse{appinit::InitDecision::kBnet});
    REQUIRE_FALSE(
        appinit::InitConnResponse{appinit::InitDecision::kBnet}
        == appinit::InitConnResponse{appinit::InitDecision::kFile});
}

// ── R168.b rate-limit tests ──────────────────────────────────────────

TEST_CASE("dispatch_init_conn: max_conns_per_ip=0 disables rate limit",
          "[application][init][dispatch][ratelimit]") {
    appinit::InitConnRequest req{bniw::kClassBnet};
    req.conn_count = 1000;
    req.max_conns_per_ip = 0;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kBnet);
}

TEST_CASE("dispatch_init_conn: under cap accepts normally",
          "[application][init][dispatch][ratelimit]") {
    appinit::InitConnRequest req{bniw::kClassBnet};
    req.conn_count = 4;
    req.max_conns_per_ip = 5;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kBnet);
}

TEST_CASE("dispatch_init_conn: at-cap accepts (strict >)",
          "[application][init][dispatch][ratelimit]") {
    appinit::InitConnRequest req{bniw::kClassBnet};
    req.conn_count = 5;
    req.max_conns_per_ip = 5;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kBnet);
}

TEST_CASE("dispatch_init_conn: over cap returns kRateLimited",
          "[application][init][dispatch][ratelimit]") {
    appinit::InitConnRequest req{bniw::kClassBnet};
    req.conn_count = 6;
    req.max_conns_per_ip = 5;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kRateLimited);
}

TEST_CASE("dispatch_init_conn: D2CS_BNETD exempt from rate limit",
          "[application][init][dispatch][ratelimit]") {
    appinit::InitConnRequest req{bniw::kClassD2csBnetd};
    req.conn_count = 1000;
    req.max_conns_per_ip = 5;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kD2csBnetd);
}

TEST_CASE("dispatch_init_conn: rate-limit precedes class lookup",
          "[application][init][dispatch][ratelimit]") {
    // Even an otherwise-rejected class returns kRateLimited when
    // the cap is exceeded -- the limit is a pre-filter.
    appinit::InitConnRequest req{bniw::kClassEnc};
    req.conn_count = 6;
    req.max_conns_per_ip = 5;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kRateLimited);
}

// ── R168.c realmlist tests ───────────────────────────────────────────

TEST_CASE("dispatch_init_conn: D2CS_BNETD with allowed IP accepts",
          "[application][init][dispatch][realmlist]") {
    appinit::InitConnRequest req{bniw::kClassD2csBnetd};
    req.d2cs_ip_allowed = true;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kD2csBnetd);
}

TEST_CASE("dispatch_init_conn: D2CS_BNETD with disallowed IP rejects",
          "[application][init][dispatch][realmlist]") {
    appinit::InitConnRequest req{bniw::kClassD2csBnetd};
    req.d2cs_ip_allowed = false;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kD2csIpDenied);
}

TEST_CASE("dispatch_init_conn: d2cs_ip_allowed=false irrelevant for non-D2CS",
          "[application][init][dispatch][realmlist]") {
    appinit::InitConnRequest req{bniw::kClassBnet};
    req.d2cs_ip_allowed = false;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kBnet);
}

TEST_CASE("dispatch_init_conn: rate-limit short-circuits realmlist gate",
          "[application][init][dispatch][realmlist]") {
    // If both gates fire, rate-limit wins (D2CS is exempt from rate-
    // limit anyway, so this construction can't actually trigger
    // both; we assert the documented precedence with a non-D2CS
    // class instead).
    appinit::InitConnRequest req{bniw::kClassBnet};
    req.conn_count = 6;
    req.max_conns_per_ip = 5;
    req.d2cs_ip_allowed = false;
    REQUIRE(appinit::dispatch_init_conn(req).decision
            == appinit::InitDecision::kRateLimited);
}
