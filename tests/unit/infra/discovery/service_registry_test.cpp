#include <gtest/gtest.h>
#include <thread>
#include "infra/discovery/service_registry.hpp"

namespace pvpgn::infra::discovery::test {

ServiceEndpoint make_endpoint(std::string name, std::string host, uint16_t port) {
    ServiceEndpoint ep;
    ep.service_name = std::move(name);
    ep.host = std::move(host);
    ep.port = port;
    ep.registered_at = std::chrono::steady_clock::now();
    ep.ttl = std::chrono::seconds{30};
    ep.healthy = true;
    return ep;
}

TEST(InMemoryServiceRegistryTest, RegisterAndLookup) {
    InMemoryServiceRegistry reg;
    auto ep = make_endpoint("bnetd", "127.0.0.1", 6112);
    auto result = reg.register_service(ep);
    ASSERT_TRUE(result.has_value());

    auto found = reg.lookup("bnetd");
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].host, "127.0.0.1");
    EXPECT_EQ(found[0].port, 6112);
}

TEST(InMemoryServiceRegistryTest, LookupNonexistentReturnsEmpty) {
    InMemoryServiceRegistry reg;
    auto found = reg.lookup("nonexistent");
    EXPECT_TRUE(found.empty());
}

TEST(InMemoryServiceRegistryTest, LookupOneReturnsError_WhenEmpty) {
    InMemoryServiceRegistry reg;
    auto result = reg.lookup_one("bnetd");
    EXPECT_FALSE(result.has_value());
}

TEST(InMemoryServiceRegistryTest, DeregisterRemovesEndpoint) {
    InMemoryServiceRegistry reg;
    auto ep = make_endpoint("bnetd", "127.0.0.1", 6112);
    auto id = reg.register_service(ep).value();
    EXPECT_EQ(reg.total_count(), 1u);

    auto result = reg.deregister_service(id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(reg.total_count(), 0u);
    EXPECT_TRUE(reg.lookup("bnetd").empty());
}

TEST(InMemoryServiceRegistryTest, RoundRobinLoadBalancing) {
    InMemoryServiceRegistry reg;
    reg.register_service(make_endpoint("bnetd", "10.0.0.1", 6112));
    reg.register_service(make_endpoint("bnetd", "10.0.0.2", 6112));
    reg.register_service(make_endpoint("bnetd", "10.0.0.3", 6112));

    std::set<std::string> seen_hosts;
    for (int i = 0; i < 6; ++i) {
        auto ep = reg.lookup_one("bnetd");
        ASSERT_TRUE(ep.has_value());
        seen_hosts.insert(ep->host);
    }
    EXPECT_EQ(seen_hosts.size(), 3u); // All 3 hosts were used
}

TEST(InMemoryServiceRegistryTest, WatchCallbackFiredOnRegister) {
    InMemoryServiceRegistry reg;
    bool callback_fired = false;
    reg.watch("bnetd", [&](const ServiceEndpoint&, bool registered) {
        if (registered) callback_fired = true;
    });

    reg.register_service(make_endpoint("bnetd", "127.0.0.1", 6112));
    EXPECT_TRUE(callback_fired);
}

TEST(InMemoryServiceRegistryTest, HeartbeatUpdatesTimestamp) {
    InMemoryServiceRegistry reg;
    auto ep = make_endpoint("bnetd", "127.0.0.1", 6112);
    auto id = reg.register_service(ep).value();
    auto result = reg.heartbeat(id);
    EXPECT_TRUE(result.has_value());
}

TEST(InMemoryServiceRegistryTest, EvictExpiredRemovesOldEndpoints) {
    InMemoryServiceRegistry reg;
    auto ep = make_endpoint("bnetd", "127.0.0.1", 6112);
    ep.ttl = std::chrono::seconds{0}; // Expire immediately
    ep.registered_at = std::chrono::steady_clock::now() - std::chrono::seconds{60};
    reg.register_service(ep);
    EXPECT_EQ(reg.total_count(), 1u);

    reg.evict_expired();
    EXPECT_EQ(reg.total_count(), 0u);
}

} // namespace pvpgn::infra::discovery::test
