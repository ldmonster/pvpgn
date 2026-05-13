// SPDX-License-Identifier: GPL-2.0-or-later
//
// Multi-threaded fiber pool integration test. Built only when
// PVPGN_V3_HAVE_FIBER is defined.
//
// Strategy
// --------
// Stand up a 2-worker FiberPool with an echo handler. Open 8
// concurrent loopback clients, each writes one chunk and reads it
// back. Verify all 8 round-trips succeed.
//
// We don't assert *which* worker handled which session — only that
// `worker_count() == 2` and that all sessions complete. The pool's
// round-robin distribution is a property of the implementation, not
// part of the public contract.

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <catch2/catch_test_macros.hpp>

#include "infra/net/fiber_pool.hpp"

using namespace pvpgn;
namespace asio = boost::asio;
using boost::asio::ip::tcp;

TEST_CASE("FiberPool: 2 workers serve 8 concurrent loopback echos",
          "[infra][net][fiber][pool]") {
    infra::net::FiberPool pool;
    pool.start(2);
    REQUIRE(pool.worker_count() == 2);

    auto handler = [](infra::net::fiber::SessionChannel& chan) {
        while (auto chunk = chan.recv()) {
            chan.send(std::move(*chunk));
        }
    };

    auto bound = pool.accept("127.0.0.1", 0, handler);
    REQUIRE(bound.has_value());
    const std::uint16_t port = bound.value().port();
    REQUIRE(port != 0);

    constexpr int kClients = 8;
    std::atomic<int> successes{0};
    std::vector<std::thread> clients;
    clients.reserve(kClients);

    for (int i = 0; i < kClients; ++i) {
        clients.emplace_back([port, i, &successes] {
            asio::io_context cctx;
            tcp::socket cli(cctx);
            boost::system::error_code ec;
            cli.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"),
                                      port), ec);
            if (ec) return;
            const std::string payload = "client-" + std::to_string(i);
            asio::write(cli, asio::buffer(payload), ec);
            if (ec) return;
            std::array<char, 64> rxbuf{};
            std::size_t n = asio::read(cli,
                asio::buffer(rxbuf.data(), payload.size()), ec);
            if (ec || n != payload.size()) return;
            if (std::string(rxbuf.data(), n) == payload) {
                successes.fetch_add(1);
            }
            cli.close();
        });
    }

    for (auto& t : clients) t.join();

    REQUIRE(successes.load() == kClients);

    pool.stop();
}

TEST_CASE("FiberPool: accept fails before start",
          "[infra][net][fiber][pool][error]") {
    infra::net::FiberPool pool;
    auto r = pool.accept("127.0.0.1", 0,
                         [](infra::net::fiber::SessionChannel&) {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::FailedPrecondition);
}

TEST_CASE("FiberPool: invalid bind address surfaces InvalidArgument",
          "[infra][net][fiber][pool][error]") {
    infra::net::FiberPool pool;
    pool.start(1);
    auto r = pool.accept("not-an-address", 0,
                         [](infra::net::fiber::SessionChannel&) {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
    pool.stop();
}
