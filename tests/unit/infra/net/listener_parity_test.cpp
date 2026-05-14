// SPDX-License-Identifier: GPL-2.0-or-later
//
// Listener parity tests for the v3 acceptor stack.
//
// bnetd historically uses an `addrlist` of listening addresses
// (`t_addr` entries built from prefs like `servaddrs`/`w3routeaddr`),
// dispatched through a `fdwatch`-based accept loop. Before we can
// retire `fdwatch`, the v3 `TcpAcceptor` stack must satisfy the same
// invariants the legacy loop relied on:
//
//   1. Multiple listeners can coexist on a single IoRuntime, each
//      bound to its own endpoint, accepting concurrent clients
//      without cross-talk. (legacy: one fdwatch loop, N laddrs)
//   2. A per-session pre-handler can reject connections immediately
//      after `accept()` — the legacy `ipbanlist_check()` + curt
//      `psock_shutdown`/`psock_close` path on banned peers.
//   3. Binding on `0.0.0.0:0` resolves to a concrete bound endpoint
//      observable to the application (so the prefs-loaded port can
//      be reported back / advertised).
//
// These tests pin those behaviours so the eventual fdwatch removal
// becomes a mechanical swap.
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
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

namespace {

// Connect a synchronous Asio client to `port` on loopback and run an
// echo round-trip with `payload`. Returns the echoed string (possibly
// empty if the connection was rejected immediately).
std::string client_echo_round_trip(std::uint16_t port,
                                   const std::string& payload) {
    asio::io_context cctx;
    tcp::socket cli(cctx);
    boost::system::error_code ec;
    cli.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
    if (ec) return {};
    asio::write(cli, asio::buffer(payload), ec);
    if (ec) return {};
    std::string out;
    out.resize(payload.size());
    std::size_t n = asio::read(cli, asio::buffer(out.data(), out.size()), ec);
    out.resize(n);
    cli.close();
    return out;
}

}  // namespace

TEST_CASE("listener parity: two TcpAcceptors on one IoRuntime accept independently",
          "[infra][net][listener][parity]") {
    infra::net::IoRuntime rt;

    // Two separate echo factories, each tagging its own session list.
    std::mutex live_mu;
    std::vector<std::shared_ptr<infra::net::TcpSession>> live;

    auto make_echo_factory = [&] {
        return [&](std::shared_ptr<infra::net::TcpSession> s) {
            s->set_on_bytes([s_wk = std::weak_ptr{s}](core::ByteView v) {
                if (auto sp = s_wk.lock()) {
                    sp->send(std::vector<std::byte>(v.begin(), v.end()));
                }
            });
            {
                std::scoped_lock lk{live_mu};
                live.push_back(s);
            }
            s->start();
        };
    };

    infra::net::TcpAcceptor acc_a(rt, make_echo_factory());
    infra::net::TcpAcceptor acc_b(rt, make_echo_factory());

    auto a = acc_a.listen("127.0.0.1", 0);
    auto b = acc_b.listen("127.0.0.1", 0);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    REQUIRE(a.value().port() != b.value().port());

    rt.run(2);

    REQUIRE(client_echo_round_trip(a.value().port(), "hi-a") == "hi-a");
    REQUIRE(client_echo_round_trip(b.value().port(), "hi-b") == "hi-b");

    {
        std::scoped_lock lk{live_mu};
        for (auto& s : live) s->close();
    }
    acc_a.close();
    acc_b.close();
    rt.stop();
}

TEST_CASE("listener parity: factory can reject a connection at accept time",
          "[infra][net][listener][parity]") {
    infra::net::IoRuntime rt;

    std::atomic<int> accepted{0};
    std::atomic<int> rejected{0};

    infra::net::TcpAcceptor acc(rt, [&](std::shared_ptr<infra::net::TcpSession> s) {
        // Simulate ipbanlist_check() refusing the peer: close the
        // session straight away without ever calling start(). The
        // OS-level accept already returned, so the client sees a
        // graceful close shortly after `connect()` succeeds.
        rejected.fetch_add(1, std::memory_order_relaxed);
        s->close();
        accepted.fetch_add(1, std::memory_order_relaxed);
    });

    auto bound = acc.listen("127.0.0.1", 0);
    REQUIRE(bound.has_value());
    const std::uint16_t port = bound.value().port();
    rt.run(1);

    // The connect() itself succeeds (the OS accepted in the listen
    // backlog before our factory closes), but the immediately-following
    // read should fail or yield 0 bytes.
    asio::io_context cctx;
    tcp::socket cli(cctx);
    boost::system::error_code ec;
    cli.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
    REQUIRE_FALSE(ec);

    std::array<char, 8> buf{};
    std::size_t n = cli.read_some(asio::buffer(buf), ec);
    REQUIRE((n == 0 || ec));   // either EOF (n==0) or error

    cli.close();

    // Give the runtime a moment to record the accept callback.
    for (int i = 0; i < 50 && accepted.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(accepted.load() == 1);
    REQUIRE(rejected.load() == 1);

    acc.close();
    rt.stop();
}

TEST_CASE("listener parity: 0.0.0.0:0 resolves to a concrete bound port",
          "[infra][net][listener][parity]") {
    infra::net::IoRuntime rt;
    infra::net::TcpAcceptor acc(rt, [](auto) {});
    auto bound = acc.listen("0.0.0.0", 0);
    REQUIRE(bound.has_value());
    REQUIRE(bound.value().port() != 0);
    REQUIRE(bound.value().address().is_unspecified());

    // local_endpoint() must agree with the value returned by listen().
    auto le = acc.local_endpoint();
    REQUIRE(le.port() == bound.value().port());

    acc.close();
}
