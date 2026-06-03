// SPDX-License-Identifier: GPL-2.0-or-later

/// @file asio_event_loop_test.cpp
/// Unit tests for `AsioEventLoop`.
///
/// Test cases
/// ----------
///  1.  `run_for()` returns within a reasonable time bound (no pending work)
///  2.  `run_for()` returns within a reasonable time bound (with pending work)
///  3.  `post()` executes the callback on the io_context
///  4.  `post()` is safe to call from a different thread
///  5.  `stop()` causes `run()` to return
///  6.  `io_context()` returns a reference to the underlying context
///  7.  Multiple `run_for()` calls work correctly (context restarts)
///  8.  `post()` from multiple threads — all callbacks execute
///  9.  `run_for()` with zero budget polls without blocking

#include "app/bnetd/asio_event_loop.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <boost/asio/post.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pvpgn::app::bnetd;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helper: measure wall-clock duration of a callable
// ---------------------------------------------------------------------------
template <class Fn>
static std::chrono::milliseconds measure(Fn&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
}

// ===========================================================================
// AsioEventLoop tests
// ===========================================================================

TEST_CASE("AsioEventLoop: run_for returns within time bound (no pending work)",
          "[asio_event_loop]") {
    AsioEventLoop loop;

    // With no pending work the context should return quickly after the budget.
    const auto budget = 50ms;
    const auto elapsed = measure([&] { loop.run_for(budget); });

    // Allow generous upper bound (CI machines can be slow).
    REQUIRE(elapsed < budget + 500ms);
}

TEST_CASE("AsioEventLoop: run_for returns within time bound (with pending work)",
          "[asio_event_loop]") {
    AsioEventLoop loop;

    // Post a handler that sleeps longer than the budget.
    // run_for() should still return within a reasonable time.
    loop.post([] {
        std::this_thread::sleep_for(200ms);
    });

    const auto budget = 50ms;
    const auto elapsed = measure([&] { loop.run_for(budget); });

    // The handler may run and extend the time, but it should not block forever.
    REQUIRE(elapsed < budget + 1000ms);
}

TEST_CASE("AsioEventLoop: post executes callback on io_context",
          "[asio_event_loop]") {
    AsioEventLoop loop;
    std::atomic<bool> executed{false};

    loop.post([&executed] { executed.store(true, std::memory_order_release); });

    // run_for gives the handler a chance to execute.
    loop.run_for(200ms);

    REQUIRE(executed.load(std::memory_order_acquire));
}

TEST_CASE("AsioEventLoop: post from a different thread executes callback",
          "[asio_event_loop]") {
    AsioEventLoop loop;
    std::atomic<int> counter{0};

    // Post from a background thread.
    std::thread poster([&loop, &counter] {
        loop.post([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    });
    poster.join();

    loop.run_for(200ms);

    REQUIRE(counter.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("AsioEventLoop: stop causes run() to return", "[asio_event_loop]") {
    AsioEventLoop loop;
    std::atomic<bool> run_returned{false};

    // Schedule stop() to fire after a short delay from a background thread.
    std::thread stopper([&loop] {
        std::this_thread::sleep_for(50ms);
        loop.stop();
    });

    // run() should block until stop() is called.
    const auto t0 = std::chrono::steady_clock::now();
    loop.run();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0);

    run_returned.store(true, std::memory_order_release);
    stopper.join();

    REQUIRE(run_returned.load(std::memory_order_acquire));
    // Should have returned within a generous bound after stop().
    REQUIRE(elapsed < 2000ms);
}

TEST_CASE("AsioEventLoop: io_context() returns underlying context reference",
          "[asio_event_loop]") {
    AsioEventLoop loop;

    // We can obtain the io_context and post directly to it.
    std::atomic<bool> executed{false};
    boost::asio::post(loop.io_context(),
                      [&executed] {
                          executed.store(true, std::memory_order_release);
                      });

    loop.run_for(200ms);

    REQUIRE(executed.load(std::memory_order_acquire));
}

TEST_CASE("AsioEventLoop: multiple run_for calls work correctly",
          "[asio_event_loop]") {
    AsioEventLoop loop;
    int call_count = 0;

    // Post three handlers.
    loop.post([&call_count] { ++call_count; });
    loop.post([&call_count] { ++call_count; });
    loop.post([&call_count] { ++call_count; });

    // First tick — should process all three (they are already queued).
    loop.run_for(200ms);
    REQUIRE(call_count == 3);

    // Post one more and tick again.
    loop.post([&call_count] { ++call_count; });
    loop.run_for(200ms);
    REQUIRE(call_count == 4);
}

TEST_CASE("AsioEventLoop: run_for with zero budget polls without blocking",
          "[asio_event_loop]") {
    AsioEventLoop loop;
    std::atomic<bool> executed{false};

    loop.post([&executed] { executed.store(true, std::memory_order_release); });

    // Zero-millisecond budget: poll mode.
    const auto elapsed = measure([&] { loop.run_for(0ms); });

    // Should return almost immediately.
    REQUIRE(elapsed < 500ms);
    // The handler may or may not have run (implementation-defined for 0ms),
    // but the call must not block.
}

TEST_CASE("AsioEventLoop: post from multiple threads — all callbacks execute",
          "[asio_event_loop]") {
    AsioEventLoop loop;

    constexpr int kThreads = 8;
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&loop, &counter] {
            loop.post([&counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        });
    }
    for (auto& t : threads) t.join();

    // Give the io_context enough time to drain all posted handlers.
    loop.run_for(500ms);

    REQUIRE(counter.load(std::memory_order_relaxed) == kThreads);
}
