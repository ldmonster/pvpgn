// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for InMemoryEventBus, with emphasis on the thread-safety fix
// (finding H2): publish/subscribe/unsubscribe are guarded by a mutex,
// and handlers are invoked OUTSIDE the lock so a re-entrant handler
// cannot deadlock.

#include <atomic>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/event_bus.hpp"

using namespace pvpgn;

namespace {

domain::events::DomainEvent make_event(std::uint32_t id) {
    return domain::events::UserLoggedOut{domain::AccountId{id}};
}

}  // namespace

TEST_CASE("InMemoryEventBus delivers published events to a subscriber",
          "[infra][inmemory][event_bus]") {
    infra::inmemory::InMemoryEventBus bus;

    int hits = 0;
    auto id  = bus.subscribe(
        [&](const domain::events::DomainEvent&) { ++hits; });

    bus.publish(make_event(1));
    bus.publish(make_event(2));

    REQUIRE(hits == 2);
    (void)id;
}

TEST_CASE("InMemoryEventBus fans out to multiple subscribers",
          "[infra][inmemory][event_bus]") {
    infra::inmemory::InMemoryEventBus bus;

    int a = 0, b = 0, c = 0;
    bus.subscribe([&](const domain::events::DomainEvent&) { ++a; });
    bus.subscribe([&](const domain::events::DomainEvent&) { ++b; });
    bus.subscribe([&](const domain::events::DomainEvent&) { ++c; });

    bus.publish(make_event(7));

    REQUIRE(a == 1);
    REQUIRE(b == 1);
    REQUIRE(c == 1);
}

TEST_CASE("InMemoryEventBus stops delivery after unsubscribe",
          "[infra][inmemory][event_bus]") {
    infra::inmemory::InMemoryEventBus bus;

    int hits = 0;
    auto id  = bus.subscribe(
        [&](const domain::events::DomainEvent&) { ++hits; });

    bus.publish(make_event(1));
    REQUIRE(hits == 1);

    bus.unsubscribe(id);
    bus.publish(make_event(2));
    REQUIRE(hits == 1);  // no further delivery
}

TEST_CASE("InMemoryEventBus isolates a throwing handler",
          "[infra][inmemory][event_bus]") {
    infra::inmemory::InMemoryEventBus bus;

    int good = 0;
    bus.subscribe([](const domain::events::DomainEvent&) {
        throw std::runtime_error("bad handler");
    });
    bus.subscribe([&](const domain::events::DomainEvent&) { ++good; });

    REQUIRE_NOTHROW(bus.publish(make_event(1)));
    REQUIRE(good == 1);
}

// The key regression for the H2 fix: the snapshot is taken UNDER the
// lock, but handlers are invoked OUTSIDE it. A handler that re-enters
// the bus (publish/subscribe/unsubscribe) during dispatch must NOT
// deadlock. With a non-recursive mutex held across dispatch this test
// would hang forever; releasing the lock before dispatch makes it pass.
TEST_CASE("InMemoryEventBus handler re-entrancy does not deadlock",
          "[infra][inmemory][event_bus]") {
    infra::inmemory::InMemoryEventBus bus;

    int outer = 0;
    int inner = 0;

    // A second subscriber that the re-entrant publish will reach.
    bus.subscribe([&](const domain::events::DomainEvent&) { ++inner; });

    bus.subscribe([&](const domain::events::DomainEvent& e) {
        ++outer;
        // Re-enter the bus from inside a handler. Subscribe + publish
        // both take the mutex; if the lock were held during dispatch
        // this would deadlock.
        if (std::holds_alternative<domain::events::UserLoggedOut>(e) &&
            std::get<domain::events::UserLoggedOut>(e).id
                == domain::AccountId{100}) {
            bus.subscribe(
                [](const domain::events::DomainEvent&) {});
            bus.publish(make_event(200));  // does NOT recurse on id 100
        }
    });

    REQUIRE_NOTHROW(bus.publish(make_event(100)));
    REQUIRE(outer >= 1);
    REQUIRE(inner >= 1);
}

// Concurrency smoke test: many threads publish while another thread
// churns subscribe/unsubscribe. Pre-fix this races the handler map and
// trips TSan / crashes; post-fix it must complete cleanly. We only
// assert it does not crash/hang and that some deliveries happened.
TEST_CASE("InMemoryEventBus concurrent publish/subscribe is race-free",
          "[infra][inmemory][event_bus][concurrency]") {
    infra::inmemory::InMemoryEventBus bus;

    std::atomic<std::uint64_t> deliveries{0};
    std::atomic<bool>          stop{false};

    // A long-lived subscriber so publishes have something to deliver.
    bus.subscribe([&](const domain::events::DomainEvent&) {
        deliveries.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int kPublishers = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < kPublishers; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 2000; ++i) {
                bus.publish(make_event(static_cast<std::uint32_t>(i)));
            }
        });
    }

    // Churn thread: rapidly subscribe and unsubscribe.
    threads.emplace_back([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            auto id = bus.subscribe([&](const domain::events::DomainEvent&) {
                deliveries.fetch_add(1, std::memory_order_relaxed);
            });
            bus.unsubscribe(id);
        }
    });

    for (int t = 0; t < kPublishers; ++t) {
        threads[static_cast<std::size_t>(t)].join();
    }
    stop.store(true, std::memory_order_relaxed);
    threads.back().join();

    // The long-lived subscriber alone guarantees at least this many.
    REQUIRE(deliveries.load() >= static_cast<std::uint64_t>(kPublishers) * 2000);
}
