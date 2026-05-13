// SPDX-License-Identifier: GPL-2.0-or-later
#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "core/clock.hpp"

using namespace pvpgn::core;
using namespace std::chrono_literals;

TEST_CASE("SystemClock advances", "[core][clock]") {
    SystemClock c;
    const auto a = c.monotonic();
    const auto b = c.monotonic();
    REQUIRE(b >= a);
}

TEST_CASE("ManualClock is frozen", "[core][clock]") {
    ManualClock c;
    const auto a = c.now();
    const auto b = c.now();
    REQUIRE(a == b);
}

TEST_CASE("ManualClock::advance moves both clocks", "[core][clock]") {
    ManualClock c;
    const auto m0 = c.monotonic();
    const auto s0 = c.now();
    c.advance(500ms);
    REQUIRE(c.monotonic() - m0 == 500ms);
    REQUIRE(c.now() - s0 == 500ms);
}

TEST_CASE("ManualClock::set overrides wall-clock only", "[core][clock]") {
    ManualClock c;
    IClock::SystemTime t = std::chrono::system_clock::time_point{} + 1h;
    c.set(t);
    REQUIRE(c.now() == t);
}
