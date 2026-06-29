// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for app::d2cs::D2gsRegistry — the cross-session game-lobby
// rendezvous. Exercises the correlation map, game-id allocation, and the
// empty/expired-link choose path without constructing real sessions (the
// registry only holds weak_ptr<D2CSTcpSession>, so empty weak refs suffice).

#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "app/d2cs/d2gs_registry.hpp"

using pvpgn::app::d2cs::D2gsRegistry;
using pvpgn::app::d2cs::D2CSTcpSession;

TEST_CASE("D2gsRegistry: pending requests correlate and are taken once",
          "[app][d2cs][registry]") {
    D2gsRegistry reg;

    const std::uint32_t c1 =
        reg.add_pending(std::weak_ptr<D2CSTcpSession>{}, 0x1111, "GameA");
    const std::uint32_t c2 =
        reg.add_pending(std::weak_ptr<D2CSTcpSession>{}, 0x2222, "GameB");
    CHECK(c1 != c2);  // correlation ids are unique

    auto p2 = reg.take_pending(c2);
    REQUIRE(p2.has_value());
    CHECK(p2->client_seqno == 0x2222);
    CHECK(p2->game_name == "GameB");

    // Taken once — a second take of the same id fails.
    CHECK_FALSE(reg.take_pending(c2).has_value());

    auto p1 = reg.take_pending(c1);
    REQUIRE(p1.has_value());
    CHECK(p1->client_seqno == 0x1111);
    CHECK(p1->game_name == "GameA");

    // Unknown id -> nullopt.
    CHECK_FALSE(reg.take_pending(0xDEADBEEF).has_value());
}

TEST_CASE("D2gsRegistry: game ids are monotonic from 1",
          "[app][d2cs][registry]") {
    D2gsRegistry reg;
    CHECK(reg.next_game_id() == 1);
    CHECK(reg.next_game_id() == 2);
    CHECK(reg.next_game_id() == 3);
}

TEST_CASE("D2gsRegistry: choose returns null when no live D2GS is registered",
          "[app][d2cs][registry]") {
    D2gsRegistry reg;
    CHECK(reg.choose_d2gs() == nullptr);  // none registered

    // Expired links (default/empty weak_ptrs) are skipped + pruned.
    reg.add_d2gs(std::weak_ptr<D2CSTcpSession>{});
    reg.add_d2gs(std::weak_ptr<D2CSTcpSession>{});
    CHECK(reg.choose_d2gs() == nullptr);

    // remove_d2gs on a null/unknown pointer is a no-op (must not crash).
    reg.remove_d2gs(nullptr);
    CHECK(reg.choose_d2gs() == nullptr);
}
