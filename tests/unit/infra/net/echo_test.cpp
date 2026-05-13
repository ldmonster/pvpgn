// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
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

#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include "infra/net/tcp_session.hpp"

using namespace pvpgn;
namespace asio = boost::asio;
using boost::asio::ip::tcp;

TEST_CASE("echo: TcpAcceptor + TcpSession round-trip on loopback",
          "[infra][net][integration]") {
    infra::net::IoRuntime rt;

    // Echo factory: every accepted session bounces incoming bytes back.
    std::vector<std::shared_ptr<infra::net::TcpSession>> live_sessions;
    std::mutex live_mu;

    infra::net::TcpAcceptor acc(rt, [&](std::shared_ptr<infra::net::TcpSession> s) {
        s->set_on_bytes([s_wk = std::weak_ptr{s}](core::ByteView v) {
            if (auto sp = s_wk.lock()) {
                sp->send(std::vector<std::byte>(v.begin(), v.end()));
            }
        });
        {
            std::scoped_lock lk{live_mu};
            live_sessions.push_back(s);
        }
        s->start();
    });

    auto bound = acc.listen("127.0.0.1", 0);
    REQUIRE(bound.has_value());
    const std::uint16_t port = bound.value().port();
    REQUIRE(port != 0);

    rt.run(2);

    // Client: synchronous Asio in this thread.
    asio::io_context cctx;
    tcp::socket cli(cctx);
    boost::system::error_code ec;
    cli.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
    REQUIRE_FALSE(ec);

    const std::string payload = "hello pvpgn";
    asio::write(cli, asio::buffer(payload), ec);
    REQUIRE_FALSE(ec);

    std::array<char, 64> rxbuf{};
    std::size_t n = asio::read(cli, asio::buffer(rxbuf.data(), payload.size()), ec);
    REQUIRE_FALSE(ec);
    REQUIRE(n == payload.size());
    REQUIRE(std::string(rxbuf.data(), n) == payload);

    cli.close();

    // Tear down.
    {
        std::scoped_lock lk{live_mu};
        for (auto& s : live_sessions) s->close();
    }
    acc.close();
    rt.stop();
}

TEST_CASE("IoRuntime: post executes on a worker thread",
          "[infra][net][io_runtime]") {
    infra::net::IoRuntime rt;
    rt.run(1);

    std::mutex mu;
    std::condition_variable cv;
    bool done = false;
    std::thread::id who;

    rt.post([&] {
        std::scoped_lock lk{mu};
        who  = std::this_thread::get_id();
        done = true;
        cv.notify_one();
    });

    std::unique_lock lk{mu};
    REQUIRE(cv.wait_for(lk, std::chrono::seconds(2), [&] { return done; }));
    REQUIRE(who != std::this_thread::get_id());
    rt.stop();
}

TEST_CASE("TcpAcceptor: invalid bind address surfaces InvalidArgument",
          "[infra][net][error]") {
    infra::net::IoRuntime rt;
    infra::net::TcpAcceptor acc(rt, [](auto) {});
    auto r = acc.listen("not-an-address", 0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}
