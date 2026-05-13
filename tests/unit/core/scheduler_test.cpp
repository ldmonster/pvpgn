// SPDX-License-Identifier: GPL-2.0-or-later
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "core/scheduler.hpp"

using namespace pvpgn::core;
using namespace std::chrono_literals;

TEST_CASE("ManualScheduler fires timer after deadline", "[core][scheduler]") {
    auto clock = std::make_shared<ManualClock>();
    ManualScheduler sched(clock);

    int hits = 0;
    sched.schedule_after(10ms, [&] { ++hits; });

    REQUIRE(sched.advance(5ms) == 0);
    REQUIRE(hits == 0);
    REQUIRE(sched.advance(10ms) == 1);
    REQUIRE(hits == 1);
}

TEST_CASE("ManualScheduler cancels pending timer", "[core][scheduler]") {
    auto clock = std::make_shared<ManualClock>();
    ManualScheduler sched(clock);

    int hits = 0;
    auto id = sched.schedule_after(10ms, [&] { ++hits; });
    sched.cancel(id);
    REQUIRE(sched.advance(20ms) == 0);
    REQUIRE(hits == 0);
}

TEST_CASE("ManualScheduler fires multiple timers in order", "[core][scheduler]") {
    auto clock = std::make_shared<ManualClock>();
    ManualScheduler sched(clock);

    int seq = 0, a_at = 0, b_at = 0, c_at = 0;
    sched.schedule_after(20ms, [&] { c_at = ++seq; });
    sched.schedule_after( 5ms, [&] { a_at = ++seq; });
    sched.schedule_after(10ms, [&] { b_at = ++seq; });

    REQUIRE(sched.advance(100ms) == 3);
    REQUIRE(a_at == 1);
    REQUIRE(b_at == 2);
    REQUIRE(c_at == 3);
}
