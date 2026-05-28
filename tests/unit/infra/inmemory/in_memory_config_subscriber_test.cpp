// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_config_subscriber.hpp"
#include "infra/config/server_config.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryConfigSubscriber: ReloadCountZeroInitially", "[infra][inmemory]") {
    InMemoryConfigSubscriber sub;
    REQUIRE(sub.reload_count() == 0);
}

TEST_CASE("InMemoryConfigSubscriber: NotifyIncrementsReloadCount", "[infra][inmemory]") {
    InMemoryConfigSubscriber sub;
    infra::config::ServerConfig cfg;

    sub.notify(cfg);
    REQUIRE(sub.reload_count() == 1);

    sub.notify(cfg);
    REQUIRE(sub.reload_count() == 2);
}

TEST_CASE("InMemoryConfigSubscriber: OnConfigReloadedIncrementsCount", "[infra][inmemory]") {
    InMemoryConfigSubscriber sub;
    infra::config::ServerConfig cfg;

    sub.on_config_reloaded(cfg);
    REQUIRE(sub.reload_count() == 1);
}

TEST_CASE("InMemoryConfigSubscriber: OnReloadCallbackIsInvoked", "[infra][inmemory]") {
    InMemoryConfigSubscriber sub;
    infra::config::ServerConfig cfg;
    cfg.servername = "TestServer";

    std::string captured_name;
    sub.on_reload([&](const infra::config::ServerConfig& c) {
        captured_name = c.servername;
    });

    sub.notify(cfg);
    REQUIRE(captured_name == "TestServer");
}

TEST_CASE("InMemoryConfigSubscriber: MultipleCallbacksAreAllInvoked", "[infra][inmemory]") {
    InMemoryConfigSubscriber sub;
    infra::config::ServerConfig cfg;

    int call_count = 0;
    sub.on_reload([&](const infra::config::ServerConfig&) { ++call_count; });
    sub.on_reload([&](const infra::config::ServerConfig&) { ++call_count; });
    sub.on_reload([&](const infra::config::ServerConfig&) { ++call_count; });

    sub.notify(cfg);
    REQUIRE(call_count == 3);
}

TEST_CASE("InMemoryConfigSubscriber: CallbackReceivesCorrectConfig", "[infra][inmemory]") {
    InMemoryConfigSubscriber sub;
    infra::config::ServerConfig cfg;
    cfg.servername = "MyPvPGN";

    std::string received;
    sub.on_reload([&](const infra::config::ServerConfig& c) {
        received = c.servername;
    });

    sub.notify(cfg);
    REQUIRE(received == "MyPvPGN");
}

TEST_CASE("InMemoryConfigSubscriber: MultipleNotifiesIncrementCountCorrectly", "[infra][inmemory]") {
    InMemoryConfigSubscriber sub;
    infra::config::ServerConfig cfg;

    for (int i = 0; i < 5; ++i) {
        sub.notify(cfg);
    }
    REQUIRE(sub.reload_count() == 5);
}

}  // namespace pvpgn::infra::inmemory
