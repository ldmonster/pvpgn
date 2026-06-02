// SPDX-License-Identifier: GPL-2.0-or-later
//
// Per-session fiber unit + integration tests. Built only when
// PVPGN_V3_HAVE_FIBER is defined (Boost.Fiber enabled).
//
// Two layers exercised here:
//
//   1. SessionChannel semantics in isolation (single-thread, two
//      cooperating fibers driven by `boost::this_fiber::yield`).
//   2. Full Asio↔Fiber integration: an IoRuntime running with the
//      `round_robin` fiber scheduler installed, an echo handler
//      written as a synchronous read loop, and a real loopback TCP
//      client.
//
// Layer 2 only makes sense when the runtime is run with
// `install_fiber_scheduler=true`. The runtime forces single-thread
// in that mode (round_robin's io_context::service is per-context).

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/fiber/all.hpp>

#include <catch2/catch_test_macros.hpp>

#include "infra/net/fiber_session.hpp"
#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include "infra/net/tcp_session.hpp"

using namespace pvpgn;
namespace asio = boost::asio;
using boost::asio::ip::tcp;

namespace {
std::vector<std::byte> bytes_of(std::string_view s) {
    std::vector<std::byte> out(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        out[i] = static_cast<std::byte>(s[i]);
    }
    return out;
}
}  // namespace

TEST_CASE("SessionChannel: fiber recv sees pushed chunks in order",
          "[infra][net][fiber]") {
    // Channel without a real session — send() is a no-op when the
    // weak_ptr is empty, which we don't exercise here.
    infra::net::fiber::SessionChannel chan(/*session*/ nullptr,
                                           /*capacity*/ 4);

    std::vector<std::string> received;

    boost::fibers::fiber consumer([&] {
        while (auto chunk = chan.recv()) {
            received.emplace_back(reinterpret_cast<const char*>(chunk->data()),
                                  chunk->size());
        }
    });

    chan.push_from_network(bytes_of("alpha"));
    chan.push_from_network(bytes_of("beta"));
    chan.push_from_network(bytes_of("gamma"));

    // Yield to let the consumer drain.
    boost::this_fiber::yield();
    boost::this_fiber::yield();
    boost::this_fiber::yield();

    chan.close_inbox();
    consumer.join();

    REQUIRE(received.size() == 3);
    REQUIRE(received[0] == "alpha");
    REQUIRE(received[1] == "beta");
    REQUIRE(received[2] == "gamma");
    REQUIRE(chan.dropped() == 0);
}

TEST_CASE("SessionChannel: recv returns nullopt after close_inbox",
          "[infra][net][fiber]") {
    infra::net::fiber::SessionChannel chan(nullptr, 4);

    bool saw_null = false;
    boost::fibers::fiber consumer([&] {
        auto first = chan.recv();
        REQUIRE(first.has_value());
        auto second = chan.recv();
        if (!second.has_value()) saw_null = true;
    });

    chan.push_from_network(bytes_of("only"));
    boost::this_fiber::yield();
    chan.close_inbox();
    consumer.join();

    REQUIRE(saw_null);
}

TEST_CASE("SessionChannel: full channel increments dropped counter",
          "[infra][net][fiber]") {
    // Boost.Fiber buffered_channel stores `capacity - 1` items in its
    // ring buffer, so capacity=4 ⇒ at most 3 in flight before the
    // next push must drop.
    infra::net::fiber::SessionChannel chan(nullptr, /*capacity*/ 4);

    chan.push_from_network(bytes_of("a"));
    chan.push_from_network(bytes_of("b"));
    chan.push_from_network(bytes_of("c"));
    // No fiber draining yet → next two try_pushes must drop.
    chan.push_from_network(bytes_of("d"));
    chan.push_from_network(bytes_of("e"));

    REQUIRE(chan.dropped() == 2);

    std::vector<std::string> got;
    boost::fibers::fiber consumer([&] {
        while (auto chunk = chan.recv()) {
            got.emplace_back(reinterpret_cast<const char*>(chunk->data()),
                             chunk->size());
        }
    });
    boost::this_fiber::yield();
    chan.close_inbox();
    consumer.join();

    REQUIRE(got.size() == 3);
    REQUIRE(got[0] == "a");
    REQUIRE(got[1] == "b");
    REQUIRE(got[2] == "c");
}

// ---- Layer 2: real Asio↔Fiber loopback ---------------------------------

TEST_CASE("spawn_session: echoes loopback bytes via round_robin scheduler",
          "[infra][net][fiber][integration]") {
    infra::net::IoRuntime rt;

    // Fiber-style echo handler.
    auto handler = [](infra::net::fiber::SessionChannel& chan) {
        while (auto chunk = chan.recv()) {
            chan.send(std::move(*chunk));
        }
    };

    std::mutex live_mu;
    std::vector<std::shared_ptr<infra::net::TcpSession>> live;

    infra::net::TcpAcceptor acc(rt,
        [&, handler](std::shared_ptr<infra::net::TcpSession> s) {
            {
                std::scoped_lock lk{live_mu};
                live.push_back(s);
            }
            infra::net::fiber::spawn_session(rt, std::move(s), handler);
        });

    auto bound = acc.listen("127.0.0.1", 0);
    REQUIRE(bound.has_value());
    const std::uint16_t port = bound.value().port();
    REQUIRE(port != 0);

    // Fiber scheduler installed; runtime is single-thread.
    rt.run(/*threads=*/1, /*install_fiber_scheduler=*/true);

    // Client side runs in the test thread with its own io_context.
    asio::io_context cctx;
    tcp::socket cli(cctx);
    boost::system::error_code ec;
    cli.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
    REQUIRE_FALSE(ec);

    const std::string payload = "fiber roundtrip";
    asio::write(cli, asio::buffer(payload), ec);
    REQUIRE_FALSE(ec);

    std::array<char, 64> rxbuf{};
    std::size_t n = asio::read(cli, asio::buffer(rxbuf.data(), payload.size()), ec);
    REQUIRE_FALSE(ec);
    REQUIRE(n == payload.size());
    REQUIRE(std::string(rxbuf.data(), n) == payload);

    cli.close();

    {
        std::scoped_lock lk{live_mu};
        for (auto& s : live) s->close();
    }
    acc.close();
    rt.stop();
}

// ---- Idle-read timeout (Plan 06 [net.timeouts] acceptance) --------------

TEST_CASE("SessionChannel: recv times out when no bytes arrive",
          "[infra][net][fiber][timeout]") {
    using namespace std::chrono_literals;
    // 50ms idle-read deadline; no session, no bytes pushed.
    infra::net::fiber::SessionChannel chan(/*session*/ nullptr,
                                           /*capacity*/ 4,
                                           /*read_timeout*/ 50ms);

    bool has_value = true;
    bool timed_out = false;
    const auto start = std::chrono::steady_clock::now();

    boost::fibers::fiber consumer([&] {
        auto r = chan.recv();         // nothing pushed → must time out
        has_value = r.has_value();
        timed_out = chan.timed_out();
    });
    consumer.join();

    const auto elapsed = std::chrono::steady_clock::now() - start;
    REQUIRE_FALSE(has_value);          // nullopt on timeout
    REQUIRE(timed_out);                // flagged as a timeout, not a close
    REQUIRE(elapsed >= 45ms);          // waited ~the configured deadline
}

TEST_CASE("SessionChannel: bytes before the deadline are not a timeout",
          "[infra][net][fiber][timeout]") {
    using namespace std::chrono_literals;
    infra::net::fiber::SessionChannel chan(nullptr, 4, /*read_timeout*/ 500ms);

    std::string got;
    bool timed_out = true;
    boost::fibers::fiber consumer([&] {
        auto r = chan.recv();
        if (r) got.assign(reinterpret_cast<const char*>(r->data()), r->size());
        timed_out = chan.timed_out();
    });

    chan.push_from_network(bytes_of("hi"));   // well within the deadline
    consumer.join();

    REQUIRE(got == "hi");
    REQUIRE_FALSE(timed_out);
}

TEST_CASE("spawn_session: idle connection is closed after the read timeout",
          "[infra][net][fiber][integration][timeout]") {
    using namespace std::chrono_literals;
    infra::net::IoRuntime rt;

    std::atomic<bool> handler_done{false};
    std::atomic<bool> handler_timed_out{false};

    auto handler = [&](infra::net::fiber::SessionChannel& chan) {
        while (auto chunk = chan.recv()) {
            chan.send(std::move(*chunk));   // echo until idle-timeout
        }
        handler_timed_out.store(chan.timed_out());
        handler_done.store(true);
    };

    std::mutex live_mu;
    std::vector<std::shared_ptr<infra::net::TcpSession>> live;

    infra::net::TcpAcceptor acc(rt,
        [&, handler](std::shared_ptr<infra::net::TcpSession> s) {
            {
                std::scoped_lock lk{live_mu};
                live.push_back(s);
            }
            // 150ms idle-read deadline.
            infra::net::fiber::spawn_session(rt, std::move(s), handler,
                                             /*inbox_capacity*/ 64,
                                             /*read_timeout*/ 150ms);
        });

    auto bound = acc.listen("127.0.0.1", 0);
    REQUIRE(bound.has_value());
    const std::uint16_t port = bound.value().port();
    REQUIRE(port != 0);

    rt.run(/*threads=*/1, /*install_fiber_scheduler=*/true);

    asio::io_context cctx;
    tcp::socket cli(cctx);
    boost::system::error_code ec;
    cli.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
    REQUIRE_FALSE(ec);

    // Send nothing. The server-side fiber should hit its idle deadline,
    // close the session, and our blocking read should observe EOF.
    const auto start = std::chrono::steady_clock::now();
    std::array<char, 16> rxbuf{};
    std::size_t n = asio::read(cli, asio::buffer(rxbuf), ec);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    REQUIRE(n == 0);                       // no data, peer closed
    REQUIRE(ec == asio::error::eof);       // clean half-close from the server
    REQUIRE(elapsed >= 120ms);             // closed ~at the deadline, not before
    REQUIRE(elapsed < 5s);                 // and not "never"

    cli.close();
    {
        std::scoped_lock lk{live_mu};
        for (auto& s : live) s->close();
    }
    acc.close();
    rt.stop();

    REQUIRE(handler_done.load());
    REQUIRE(handler_timed_out.load());     // handler observed a timeout
}
