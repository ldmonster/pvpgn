// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/integration/legacy_bnetd/anongame_lobby_bridge_test.cpp
//
// Tests the base half of the v3 anongame_lobby bridge: handler
// pointer dispatch contract, the no-handler-installed return (-1)
// for legacy fallback, args pass-through, rc propagation, and the
// install hook idempotency / no-op semantics in stage 1.

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/anongame_lobby_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct Call {
    void*    conn_ptr  = nullptr;
    unsigned game_type = 0xFFFFFFFFu;
};

Call g_last{};
int  g_rc        = 1;
int  g_call_count = 0;

int fake_handler(void* conn_ptr, unsigned game_type) {
    g_last.conn_ptr  = conn_ptr;
    g_last.game_type = game_type;
    ++g_call_count;
    return g_rc;
}

struct Reset {
    Reset() {
        ila::set_anongame_lobby_handler(nullptr);
        g_last       = {};
        g_rc         = 1;
        g_call_count = 0;
    }
    ~Reset() {
        ila::set_anongame_lobby_handler(nullptr);
    }
};

}  // namespace

TEST_CASE("anongame_lobby bridge: returns -1 without a handler",
          "[anongame_lobby][bridge]") {
    Reset r;
    int dummy = 0;
    int const rc = ::pvpgn_v3_anongame_lobby_apply(&dummy, 1u);
    REQUIRE(rc == -1);
    REQUIRE(g_call_count == 0);
}

TEST_CASE("anongame_lobby bridge: returns 0 for null conn even with handler",
          "[anongame_lobby][bridge]") {
    Reset r;
    ila::set_anongame_lobby_handler(&fake_handler);

    int const rc = ::pvpgn_v3_anongame_lobby_apply(nullptr, 7u);
    CHECK(rc == 0);
    CHECK(g_call_count == 0);  // handler MUST NOT be invoked
    CHECK(g_last.conn_ptr  == nullptr);
    CHECK(g_last.game_type == 0xFFFFFFFFu);
}

TEST_CASE("anongame_lobby bridge: forwards conn_ptr and game_type",
          "[anongame_lobby][bridge]") {
    Reset r;
    ila::set_anongame_lobby_handler(&fake_handler);
    int dummy = 0;

    int const rc = ::pvpgn_v3_anongame_lobby_apply(&dummy, 42u);
    CHECK(rc == 1);
    CHECK(g_call_count == 1);
    CHECK(g_last.conn_ptr  == static_cast<void*>(&dummy));
    CHECK(g_last.game_type == 42u);
}

TEST_CASE("anongame_lobby bridge: propagates rc=0 (handler reports failure)",
          "[anongame_lobby][bridge]") {
    Reset r;
    ila::set_anongame_lobby_handler(&fake_handler);
    g_rc = 0;
    int dummy = 0;

    int const rc = ::pvpgn_v3_anongame_lobby_apply(&dummy, 3u);
    CHECK(rc == 0);
    CHECK(g_call_count == 1);
    CHECK(g_last.conn_ptr  == static_cast<void*>(&dummy));
    CHECK(g_last.game_type == 3u);
}

TEST_CASE("anongame_lobby bridge: get_handler reflects last set",
          "[anongame_lobby][bridge]") {
    Reset r;
    CHECK(ila::get_anongame_lobby_handler() == nullptr);
    ila::set_anongame_lobby_handler(&fake_handler);
    CHECK(ila::get_anongame_lobby_handler() == &fake_handler);
    ila::set_anongame_lobby_handler(nullptr);
    CHECK(ila::get_anongame_lobby_handler() == nullptr);
}

TEST_CASE("anongame_lobby bridge: install_legacy_* is a no-op in stage 1",
          "[anongame_lobby][bridge]") {
    Reset r;
    // Stage 1 must NOT install any handler -- the legacy adapter
    // over `anongame_queue` is deferred to stage 2. After install,
    // the bridge must still return -1 so the caller falls back to
    // the legacy enqueue path.
    ila::install_legacy_anongame_lobby_handler();
    CHECK(ila::get_anongame_lobby_handler() == nullptr);

    int dummy = 0;
    int const rc = ::pvpgn_v3_anongame_lobby_apply(&dummy, 1u);
    CHECK(rc == -1);
}

TEST_CASE("anongame_lobby bridge: install hook is idempotent",
          "[anongame_lobby][bridge]") {
    Reset r;
    ila::install_legacy_anongame_lobby_handler();
    ila::install_legacy_anongame_lobby_handler();
    ila::install_legacy_anongame_lobby_handler();
    CHECK(ila::get_anongame_lobby_handler() == nullptr);
}
