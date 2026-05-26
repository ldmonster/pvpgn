// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/integration/legacy_bnetd/realm_list_bridge_test.cpp
//
// Tests the base half of the v3 realm-list bridge: the
// handler-pointer dispatch contract, the no-handler-installed
// return (-1) for legacy fallback, and the args pass-through for
// the (conn_ptr, legacy_format) tuple.

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/realm_list_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct Call {
    void* conn_ptr      = nullptr;
    int   legacy_format = -7;
};

Call g_last{};
int  g_rc = 1;

int fake_handler(void* conn_ptr, int legacy_format) {
    g_last.conn_ptr      = conn_ptr;
    g_last.legacy_format = legacy_format;
    return g_rc;
}

struct Reset {
    Reset() {
        ila::set_realm_list_handler(nullptr);
        g_last = {};
        g_rc   = 1;
    }
    ~Reset() {
        ila::set_realm_list_handler(nullptr);
    }
};

}  // namespace

TEST_CASE("realm_list bridge: returns -1 without a handler",
          "[realm_list][bridge]") {
    Reset r;
    int dummy = 0;
    int const rc = ::pvpgn_v3_realm_list_apply(&dummy, 1);
    REQUIRE(rc == -1);
}

TEST_CASE("realm_list bridge: returns 0 for null conn even with handler",
          "[realm_list][bridge]") {
    Reset r;
    ila::set_realm_list_handler(&fake_handler);

    int const rc = ::pvpgn_v3_realm_list_apply(nullptr, 1);
    CHECK(rc == 0);
    // Handler must NOT have been invoked.
    CHECK(g_last.conn_ptr      == nullptr);
    CHECK(g_last.legacy_format == -7);
}

TEST_CASE("realm_list bridge: forwards conn_ptr and legacy_format=1",
          "[realm_list][bridge]") {
    Reset r;
    ila::set_realm_list_handler(&fake_handler);
    int dummy = 0;

    int const rc = ::pvpgn_v3_realm_list_apply(&dummy, 1);
    CHECK(rc == 1);
    CHECK(g_last.conn_ptr      == static_cast<void*>(&dummy));
    CHECK(g_last.legacy_format == 1);
}

TEST_CASE("realm_list bridge: forwards conn_ptr and legacy_format=0",
          "[realm_list][bridge]") {
    Reset r;
    ila::set_realm_list_handler(&fake_handler);
    int dummy = 0;

    int const rc = ::pvpgn_v3_realm_list_apply(&dummy, 0);
    CHECK(rc == 1);
    CHECK(g_last.conn_ptr      == static_cast<void*>(&dummy));
    CHECK(g_last.legacy_format == 0);
}

TEST_CASE("realm_list bridge: propagates rc=0 (handler reports send-failure)",
          "[realm_list][bridge]") {
    Reset r;
    ila::set_realm_list_handler(&fake_handler);
    g_rc = 0;
    int dummy = 0;

    int const rc = ::pvpgn_v3_realm_list_apply(&dummy, 1);
    CHECK(rc == 0);
    CHECK(g_last.conn_ptr == static_cast<void*>(&dummy));
}

TEST_CASE("realm_list bridge: get_realm_list_handler reflects last set",
          "[realm_list][bridge]") {
    Reset r;
    CHECK(ila::get_realm_list_handler() == nullptr);
    ila::set_realm_list_handler(&fake_handler);
    CHECK(ila::get_realm_list_handler() == &fake_handler);
    ila::set_realm_list_handler(nullptr);
    CHECK(ila::get_realm_list_handler() == nullptr);
}
