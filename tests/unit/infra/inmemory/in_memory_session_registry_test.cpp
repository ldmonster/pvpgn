// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/session_registry.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemorySessionRegistry: AttachThenLookupBothDirections",
          "[infra][inmemory][session]") {
    InMemorySessionRegistry reg;
    REQUIRE(reg.attach(domain::SessionId{10}, domain::AccountId{1}).has_value());

    auto s = reg.session_for(domain::AccountId{1});
    REQUIRE(s.has_value());
    REQUIRE(s->value() == 10);

    auto a = reg.account_for(domain::SessionId{10});
    REQUIRE(a.has_value());
    REQUIRE(a->value() == 1);

    REQUIRE(reg.list().size() == 1);
}

TEST_CASE("InMemorySessionRegistry: SecondSessionForSameAccountRejected",
          "[infra][inmemory][session]") {
    InMemorySessionRegistry reg;
    REQUIRE(reg.attach(domain::SessionId{10}, domain::AccountId{1}).has_value());

    // One account == one session: a different session for the same account
    // must be rejected (the check-then-act invariant).
    auto dup = reg.attach(domain::SessionId{11}, domain::AccountId{1});
    REQUIRE_FALSE(dup.has_value());
    REQUIRE(dup.error().code() == core::StatusCode::AlreadyExists);

    // And reusing the same session id is likewise rejected.
    auto dup_sess = reg.attach(domain::SessionId{10}, domain::AccountId{2});
    REQUIRE_FALSE(dup_sess.has_value());
    REQUIRE(dup_sess.error().code() == core::StatusCode::AlreadyExists);

    REQUIRE(reg.list().size() == 1);
}

TEST_CASE("InMemorySessionRegistry: DetachClearsBothDirections",
          "[infra][inmemory][session]") {
    InMemorySessionRegistry reg;
    REQUIRE(reg.attach(domain::SessionId{10}, domain::AccountId{1}).has_value());
    reg.detach(domain::SessionId{10});

    REQUIRE_FALSE(reg.session_for(domain::AccountId{1}).has_value());
    REQUIRE_FALSE(reg.account_for(domain::SessionId{10}).has_value());
    REQUIRE(reg.list().empty());

    // After detach, the slots are free for reuse.
    REQUIRE(reg.attach(domain::SessionId{10}, domain::AccountId{1}).has_value());
}

TEST_CASE("InMemorySessionRegistry: ConcurrentAttachDetachFindSmoke",
          "[infra][inmemory][session]") {
    // Smoke test: many threads hammer attach/detach/find concurrently. The
    // goal is to exercise the locking under ThreadSanitizer / normal runs
    // without crashing or corrupting the bidirectional maps. Each thread owns
    // a disjoint key range so the final state is deterministic.
    InMemorySessionRegistry reg;

    constexpr int kThreads = 8;
    constexpr int kPerThread = 500;

    std::atomic<int> attach_failures{0};

    auto worker = [&](int t) {
        for (int i = 0; i < kPerThread; ++i) {
            const std::uint64_t key =
                static_cast<std::uint64_t>(t) * kPerThread +
                static_cast<std::uint64_t>(i);
            const auto sid = domain::SessionId{key};
            const auto aid = domain::AccountId{static_cast<std::uint32_t>(key)};

            if (!reg.attach(sid, aid).has_value()) {
                ++attach_failures;
            }
            // Concurrent reads in both directions.
            (void)reg.session_for(aid);
            (void)reg.account_for(sid);
            (void)reg.list();
            reg.detach(sid);
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        pool.emplace_back(worker, t);
    }
    for (auto& th : pool) {
        th.join();
    }

    // Disjoint keys + matched attach/detach per iteration => no leftover state
    // and no rejected attaches.
    REQUIRE(attach_failures.load() == 0);
    REQUIRE(reg.list().empty());
}

}  // namespace pvpgn::infra::inmemory
