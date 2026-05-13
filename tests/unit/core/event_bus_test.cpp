// SPDX-License-Identifier: GPL-2.0-or-later
#include <atomic>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/event_bus.hpp"

using namespace pvpgn::core;

namespace {
struct LoginEvent  { std::string user; };
struct LogoutEvent { std::string user; };
}

TEST_CASE("event_bus delivers to a single subscriber", "[core][event_bus]") {
    EventBus bus;
    std::string received;
    auto sub = bus.subscribe<LoginEvent>(
        [&](const LoginEvent& e) { received = e.user; });

    bus.publish(LoginEvent{"alice"});
    REQUIRE(received == "alice");
}

TEST_CASE("event_bus delivers to multiple subscribers", "[core][event_bus]") {
    EventBus bus;
    int hits = 0;
    auto s1 = bus.subscribe<LoginEvent>([&](const LoginEvent&) { ++hits; });
    auto s2 = bus.subscribe<LoginEvent>([&](const LoginEvent&) { ++hits; });
    auto s3 = bus.subscribe<LoginEvent>([&](const LoginEvent&) { ++hits; });

    bus.publish(LoginEvent{"x"});
    REQUIRE(hits == 3);
}

TEST_CASE("event_bus separates by type", "[core][event_bus]") {
    EventBus bus;
    int login_hits = 0, logout_hits = 0;
    auto s1 = bus.subscribe<LoginEvent>(
        [&](const LoginEvent&) { ++login_hits; });
    auto s2 = bus.subscribe<LogoutEvent>(
        [&](const LogoutEvent&) { ++logout_hits; });

    bus.publish(LoginEvent{"x"});
    bus.publish(LoginEvent{"y"});
    bus.publish(LogoutEvent{"x"});
    REQUIRE(login_hits  == 2);
    REQUIRE(logout_hits == 1);
}

TEST_CASE("event_bus subscription RAII removes handler", "[core][event_bus]") {
    EventBus bus;
    int hits = 0;
    {
        auto sub = bus.subscribe<LoginEvent>(
            [&](const LoginEvent&) { ++hits; });
        bus.publish(LoginEvent{"x"});
        REQUIRE(hits == 1);
    }
    bus.publish(LoginEvent{"x"});
    REQUIRE(hits == 1);  // not delivered after sub destroyed
}

TEST_CASE("event_bus survives a throwing subscriber", "[core][event_bus]") {
    EventBus bus;
    int  good_hits = 0;
    auto s1 = bus.subscribe<LoginEvent>(
        [](const LoginEvent&) { throw std::runtime_error("bad"); });
    auto s2 = bus.subscribe<LoginEvent>(
        [&](const LoginEvent&) { ++good_hits; });

    bus.publish(LoginEvent{"x"});
    REQUIRE(good_hits == 1);
}
