// SPDX-License-Identifier: GPL-2.0-or-later
/// @file route_registry_test.cpp
/// Unit tests for domain::connection::RouteRegistry (R288).
///
/// Test count: 12 TEST_CASEs.

#include <cstdint>
#include <span>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "application/connection/connection_fsm.hpp"
#include "domain/connection/connection_context.hpp"
#include "domain/connection/route_registry.hpp"

using namespace pvpgn::domain::connection;
using namespace pvpgn::application::connection;

// ---------------------------------------------------------------------------
// Minimal FakeContext so we can construct ConnectionFsm instances
// ---------------------------------------------------------------------------

namespace {

class FakeCtx : public IConnectionContext {
public:
    pvpgn::core::Status<> send_packet(
        std::uint8_t,
        std::span<const std::byte>) override {
        return pvpgn::core::ok();
    }
    void close() override {}
    [[nodiscard]] std::string get_remote_address() const override { return "127.0.0.1"; }
    [[nodiscard]] std::uint32_t get_session_id() const override { return 0u; }
    void on_game_created(std::uint32_t, const GameInfo&) override {}
    void on_game_joined(std::uint32_t, const GameInfo&) override {}
    void on_game_left(std::uint32_t) override {}
};

}  // namespace

// ===========================================================================
// RouteRegistry tests
// ===========================================================================

// ---------------------------------------------------------------------------
// 1. Empty registry
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: newly constructed registry is empty",
          "[route_registry]") {
    RouteRegistry reg;
    CHECK(reg.empty());
    CHECK(reg.size() == 0u);
}

// ---------------------------------------------------------------------------
// 2. find_primary on empty registry returns nullptr
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: find_primary on empty registry returns nullptr",
          "[route_registry]") {
    RouteRegistry reg;
    CHECK(reg.find_primary(0xDEADBEEFu) == nullptr);
}

// ---------------------------------------------------------------------------
// 3. register_primary then find_primary returns the registered pointer
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: register_primary then find_primary returns pointer",
          "[route_registry]") {
    FakeCtx ctx;
    ConnectionFsm fsm{ctx, 1u};

    RouteRegistry reg;
    reg.register_primary(0x12345678u, &fsm);

    REQUIRE(reg.find_primary(0x12345678u) == &fsm);
    CHECK_FALSE(reg.empty());
    CHECK(reg.size() == 1u);
}

// ---------------------------------------------------------------------------
// 4. find_primary with wrong token returns nullptr
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: find_primary with wrong token returns nullptr",
          "[route_registry]") {
    FakeCtx ctx;
    ConnectionFsm fsm{ctx, 2u};

    RouteRegistry reg;
    reg.register_primary(0xAAAAAAAAu, &fsm);

    CHECK(reg.find_primary(0xBBBBBBBBu) == nullptr);
}

// ---------------------------------------------------------------------------
// 5. unregister removes the entry
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: unregister removes the entry",
          "[route_registry]") {
    FakeCtx ctx;
    ConnectionFsm fsm{ctx, 3u};

    RouteRegistry reg;
    reg.register_primary(0xCAFEBABEu, &fsm);
    REQUIRE(reg.find_primary(0xCAFEBABEu) == &fsm);

    reg.unregister(0xCAFEBABEu);

    CHECK(reg.find_primary(0xCAFEBABEu) == nullptr);
    CHECK(reg.empty());
    CHECK(reg.size() == 0u);
}

// ---------------------------------------------------------------------------
// 6. unregister on missing token is a no-op
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: unregister on missing token is a no-op",
          "[route_registry]") {
    RouteRegistry reg;
    // Should not throw or crash
    reg.unregister(0x00000000u);
    CHECK(reg.empty());
}

// ---------------------------------------------------------------------------
// 7. Multiple tokens can be registered independently
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: multiple tokens registered independently",
          "[route_registry]") {
    FakeCtx ctx1, ctx2, ctx3;
    ConnectionFsm fsm1{ctx1, 10u};
    ConnectionFsm fsm2{ctx2, 20u};
    ConnectionFsm fsm3{ctx3, 30u};

    RouteRegistry reg;
    reg.register_primary(0x00000001u, &fsm1);
    reg.register_primary(0x00000002u, &fsm2);
    reg.register_primary(0x00000003u, &fsm3);

    CHECK(reg.size() == 3u);
    CHECK_FALSE(reg.empty());

    CHECK(reg.find_primary(0x00000001u) == &fsm1);
    CHECK(reg.find_primary(0x00000002u) == &fsm2);
    CHECK(reg.find_primary(0x00000003u) == &fsm3);
}

// ---------------------------------------------------------------------------
// 8. Unregistering one entry does not affect others
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: unregistering one entry does not affect others",
          "[route_registry]") {
    FakeCtx ctx1, ctx2;
    ConnectionFsm fsm1{ctx1, 11u};
    ConnectionFsm fsm2{ctx2, 22u};

    RouteRegistry reg;
    reg.register_primary(0xAAAA0001u, &fsm1);
    reg.register_primary(0xAAAA0002u, &fsm2);

    reg.unregister(0xAAAA0001u);

    CHECK(reg.find_primary(0xAAAA0001u) == nullptr);
    CHECK(reg.find_primary(0xAAAA0002u) == &fsm2);
    CHECK(reg.size() == 1u);
}

// ---------------------------------------------------------------------------
// 9. register_primary overwrites existing entry for same token
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: register_primary overwrites existing entry",
          "[route_registry]") {
    FakeCtx ctx1, ctx2;
    ConnectionFsm fsm1{ctx1, 41u};
    ConnectionFsm fsm2{ctx2, 42u};

    RouteRegistry reg;
    reg.register_primary(0xFFFF0000u, &fsm1);
    REQUIRE(reg.find_primary(0xFFFF0000u) == &fsm1);

    // Overwrite with fsm2
    reg.register_primary(0xFFFF0000u, &fsm2);

    CHECK(reg.find_primary(0xFFFF0000u) == &fsm2);
    CHECK(reg.size() == 1u);  // still only one entry
}

// ---------------------------------------------------------------------------
// 10. size() reflects the number of registered entries
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: size reflects number of registered entries",
          "[route_registry]") {
    FakeCtx ctx;
    ConnectionFsm fsm{ctx, 99u};

    RouteRegistry reg;
    CHECK(reg.size() == 0u);

    reg.register_primary(0x00000001u, &fsm);
    CHECK(reg.size() == 1u);

    reg.register_primary(0x00000002u, &fsm);
    CHECK(reg.size() == 2u);

    reg.unregister(0x00000001u);
    CHECK(reg.size() == 1u);

    reg.unregister(0x00000002u);
    CHECK(reg.size() == 0u);
}

// ---------------------------------------------------------------------------
// 11. Token 0 is a valid key
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: token 0 is a valid key",
          "[route_registry]") {
    FakeCtx ctx;
    ConnectionFsm fsm{ctx, 7u};

    RouteRegistry reg;
    reg.register_primary(0u, &fsm);

    REQUIRE(reg.find_primary(0u) == &fsm);
    CHECK(reg.size() == 1u);

    reg.unregister(0u);
    CHECK(reg.empty());
}

// ---------------------------------------------------------------------------
// 12. Token UINT32_MAX is a valid key
// ---------------------------------------------------------------------------

TEST_CASE("RouteRegistry: token UINT32_MAX is a valid key",
          "[route_registry]") {
    FakeCtx ctx;
    ConnectionFsm fsm{ctx, 8u};

    RouteRegistry reg;
    constexpr std::uint32_t kMax = 0xFFFFFFFFu;
    reg.register_primary(kMax, &fsm);

    REQUIRE(reg.find_primary(kMax) == &fsm);
    reg.unregister(kMax);
    CHECK(reg.empty());
}
