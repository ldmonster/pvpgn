// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the v3 init_conn_bridge C ABI. The bridge is a
// thin pass-through to `application::init::dispatch_init_conn`, so
// these tests focus on the ABI contract: out-pointer handling,
// boundary bytes, and the accept/reject return value.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "integration/legacy_bnetd/init_conn_bridge.hpp"
#include "protocol/bnet/init_wire_types.hpp"

namespace ila  = pvpgn::integration::legacy_bnetd;
namespace bniw = pvpgn::protocol::bnet::init;

TEST_CASE("init_conn_decide accepts BNET",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(bniw::kClassBnet, &out) == 1);
    REQUIRE(out == ila::kInitDecisionBnet);
}

TEST_CASE("init_conn_decide accepts FILE",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(bniw::kClassFile, &out) == 1);
    REQUIRE(out == ila::kInitDecisionFile);
}

TEST_CASE("init_conn_decide accepts BOT",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(bniw::kClassBot, &out) == 1);
    REQUIRE(out == ila::kInitDecisionBot);
}

TEST_CASE("init_conn_decide accepts TELNET",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(bniw::kClassTelnet, &out) == 1);
    REQUIRE(out == ila::kInitDecisionTelnet);
}

TEST_CASE("init_conn_decide accepts D2CS_BNETD",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(
                bniw::kClassD2csBnetd, &out) == 1);
    REQUIRE(out == ila::kInitDecisionD2csBnetd);
}

TEST_CASE("init_conn_decide rejects ENC",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(bniw::kClassEnc, &out) == 0);
    REQUIRE(out == ila::kInitDecisionRejected);
}

TEST_CASE("init_conn_decide rejects LOCALMACHINE",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(
                bniw::kClassLocalMachine, &out) == 0);
    REQUIRE(out == ila::kInitDecisionRejected);
}

TEST_CASE("init_conn_decide rejects D2GS at bnetd",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t out = 0x55;
    REQUIRE(::pvpgn_v3_init_conn_decide(bniw::kClassD2gs, &out) == 0);
    REQUIRE(out == ila::kInitDecisionRejected);
}

TEST_CASE("init_conn_decide accepts a null out pointer",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    // Caller may pass nullptr if it only cares about the
    // accept/reject classification.
    REQUIRE(::pvpgn_v3_init_conn_decide(
                bniw::kClassBnet, nullptr) == 1);
    REQUIRE(::pvpgn_v3_init_conn_decide(
                bniw::kClassEnc, nullptr) == 0);
}

TEST_CASE("init_conn_decide table size matches dispatcher",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    int accepted = 0;
    for (int b = 0; b <= 0xff; ++b) {
        std::uint8_t out = 0;
        if (::pvpgn_v3_init_conn_decide(
                static_cast<std::uint8_t>(b), &out) == 1) {
            ++accepted;
            REQUIRE(out != ila::kInitDecisionRejected);
        } else {
            REQUIRE(out == ila::kInitDecisionRejected);
        }
    }
    REQUIRE(accepted == 5);
}

// ---------------- apply ABI dispatcher tests ----------------------
//
// The apply ABI also forwards to a registered handler. The
// legacy-aware handler lives in `integration_legacy_bnetd_linked`
// (combined build); here we only verify the dispatcher's contract.

namespace {

struct FakeApply {
    static inline int            return_value = 1;
    static inline int            call_count   = 0;
    static inline void*          last_conn    = nullptr;
    static inline std::uint8_t   last_cclass  = 0;

    static void reset() noexcept {
        return_value = 1;
        call_count   = 0;
        last_conn    = nullptr;
        last_cclass  = 0;
    }

    static int handler(void* conn_ptr, std::uint8_t cclass) noexcept {
        ++call_count;
        last_conn   = conn_ptr;
        last_cclass = cclass;
        return return_value;
    }
};

struct ScopedApply {
    ila::InitConnApplyHandler prev = ila::get_init_conn_apply_handler();
    ScopedApply() noexcept {
        FakeApply::reset();
        ila::set_init_conn_apply_handler(&FakeApply::handler);
    }
    ~ScopedApply() noexcept { ila::set_init_conn_apply_handler(prev); }
};

}  // namespace

TEST_CASE("init_conn_apply rejects accepted cclass without a handler",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    auto* saved = ila::get_init_conn_apply_handler();
    ila::set_init_conn_apply_handler(nullptr);

    int dummy = 0;
    // R168.a: under v3 the bridge is authoritative for accepted
    // cclasses -- a missing handler is a startup-wiring bug, so the
    // bridge rejects (-1) instead of silently returning 0 and
    // letting the legacy switch run.
    REQUIRE(::pvpgn_v3_init_conn_apply(&dummy, bniw::kClassBnet) == -1);

    ila::set_init_conn_apply_handler(saved);
}

TEST_CASE("init_conn_apply rejects null conn pointer",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    REQUIRE(::pvpgn_v3_init_conn_apply(nullptr, bniw::kClassBnet) == 0);
    REQUIRE(FakeApply::call_count == 0);
}

TEST_CASE("init_conn_apply skips handler for rejected bytes",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    int dummy = 0;
    // The handler MUST NOT see rejected bytes -- it is only
    // installed for accepted decisions.
    REQUIRE(::pvpgn_v3_init_conn_apply(&dummy, bniw::kClassEnc) == 0);
    REQUIRE(::pvpgn_v3_init_conn_apply(&dummy, bniw::kClassD2gs) == 0);
    REQUIRE(::pvpgn_v3_init_conn_apply(&dummy, 0x00) == 0);
    REQUIRE(::pvpgn_v3_init_conn_apply(&dummy, 0xfe) == 0);
    REQUIRE(FakeApply::call_count == 0);
}

TEST_CASE("init_conn_apply forwards accepted bytes verbatim",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    int marker = 17;

    FakeApply::return_value = 1;
    REQUIRE(::pvpgn_v3_init_conn_apply(&marker, bniw::kClassBnet) == 1);
    REQUIRE(FakeApply::call_count == 1);
    REQUIRE(FakeApply::last_conn == &marker);
    REQUIRE(FakeApply::last_cclass == bniw::kClassBnet);

    REQUIRE(::pvpgn_v3_init_conn_apply(&marker, bniw::kClassD2csBnetd) == 1);
    REQUIRE(FakeApply::call_count == 2);
    REQUIRE(FakeApply::last_cclass == bniw::kClassD2csBnetd);
}

TEST_CASE("init_conn_apply propagates handler failure (-1)",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    int marker = 0;
    FakeApply::return_value = -1;  // simulate D2CS realmlist reject

    REQUIRE(::pvpgn_v3_init_conn_apply(&marker, bniw::kClassD2csBnetd) == -1);
    REQUIRE(FakeApply::call_count == 1);
}

TEST_CASE("init_conn_apply propagates handler decline (0)",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    int marker = 0;
    FakeApply::return_value = 0;  // handler decided to fall through

    REQUIRE(::pvpgn_v3_init_conn_apply(&marker, bniw::kClassBnet) == 0);
    REQUIRE(FakeApply::call_count == 1);
}

// ----- R169.a: extended ABI tests -----
TEST_CASE("init_conn_decide_ex: defaults match decide()",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t d1 = 0xff, d2 = 0xff;
    REQUIRE(::pvpgn_v3_init_conn_decide(bniw::kClassBnet, &d1) == 1);
    REQUIRE(::pvpgn_v3_init_conn_decide_ex(bniw::kClassBnet, 0u, 0u, 1, &d2) == 1);
    REQUIRE(d1 == d2);
}

TEST_CASE("init_conn_decide_ex: rate-limit over cap rejects",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t d = 0;
    REQUIRE(::pvpgn_v3_init_conn_decide_ex(bniw::kClassBnet, 6u, 5u, 1, &d) == 0);
    REQUIRE(d == 5 /*kRateLimited*/);
}

TEST_CASE("init_conn_decide_ex: D2CS exempt from rate limit",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t d = 0;
    REQUIRE(::pvpgn_v3_init_conn_decide_ex(bniw::kClassD2csBnetd, 999u, 5u, 1, &d) == 1);
    REQUIRE(d == 4 /*kD2csBnetd*/);
}

TEST_CASE("init_conn_decide_ex: D2CS with disallowed IP rejects",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    std::uint8_t d = 0;
    REQUIRE(::pvpgn_v3_init_conn_decide_ex(bniw::kClassD2csBnetd, 0u, 0u, 0, &d) == 0);
    REQUIRE(d == 6 /*kD2csIpDenied*/);
}

TEST_CASE("init_conn_apply_ex: rate-limit returns -1 without invoking handler",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_init_conn_apply_ex(&marker, bniw::kClassBnet, 6u, 5u, 1) == -1);
    REQUIRE(FakeApply::call_count == 0);
}

TEST_CASE("init_conn_apply_ex: D2CS realmlist deny returns -1 without invoking handler",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_init_conn_apply_ex(&marker, bniw::kClassD2csBnetd, 0u, 0u, 0) == -1);
    REQUIRE(FakeApply::call_count == 0);
}

TEST_CASE("init_conn_apply_ex: allowed cclass flows to handler verbatim",
          "[integration][legacy_bnetd][init_conn_bridge]") {
    ScopedApply scope;
    int marker = 42;
    FakeApply::return_value = 1;
    REQUIRE(::pvpgn_v3_init_conn_apply_ex(&marker, bniw::kClassD2csBnetd, 3u, 5u, 1) == 1);
    REQUIRE(FakeApply::call_count == 1);
    REQUIRE(FakeApply::last_conn == &marker);
    REQUIRE(FakeApply::last_cclass == bniw::kClassD2csBnetd);
}
