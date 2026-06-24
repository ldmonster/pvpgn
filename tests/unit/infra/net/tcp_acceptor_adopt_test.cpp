// SPDX-License-Identifier: GPL-2.0-or-later
//
// Verifies that `TcpAcceptor::adopt_native_handle()` can take over a
// listening socket opened, bound, and `listen()`-ed externally -- the
// path used by `integration_legacy_bnetd_linked::TcpBridge`
// to take over the bnetd TCP accept loop without disturbing legacy
// connection setup.

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <catch2/catch_test_macros.hpp>

#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"

using namespace pvpgn;
namespace asio = boost::asio;
using boost::asio::ip::tcp;

TEST_CASE("TcpAcceptor::adopt_native_handle takes over an external listener",
          "[infra][net][adopt]") {
    // 1. Externally open + bind + listen on an ephemeral loopback port.
    asio::io_context tmp;  // throw-away io_context for the bind/listen plumbing
    tcp::acceptor seed(tmp);
    boost::system::error_code ec;
    seed.open(tcp::v4(), ec);                                  REQUIRE_FALSE(ec);
    seed.set_option(asio::socket_base::reuse_address(true), ec); REQUIRE_FALSE(ec);
    seed.bind(tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0), ec);
    REQUIRE_FALSE(ec);
    seed.listen(8, ec);                                        REQUIRE_FALSE(ec);
    const std::uint16_t port = seed.local_endpoint().port();
    REQUIRE(port != 0);

    // Release the native fd so the seed acceptor no longer owns it.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)  // acceptor::release: deprecated on pre-8.1 Windows
#endif
    auto native = seed.release(ec);
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
    REQUIRE_FALSE(ec);

    // 2. Adopt the listening fd into a TcpAcceptor with a raw handler.
    infra::net::IoRuntime rt;

    std::mutex                                       mu;
    std::condition_variable                          cv;
    std::atomic<int>                                 accepted{0};
    std::vector<std::uint16_t>                       peer_ports;

    infra::net::TcpAcceptor acc(rt, infra::net::TcpAcceptor::SessionFactory{});
    acc.set_raw_handler([&](tcp::socket sock) {
        boost::system::error_code rec;
        const auto remote = sock.remote_endpoint(rec);
        if (!rec) {
            std::scoped_lock lk{mu};
            peer_ports.push_back(remote.port());
        }
        sock.close(rec);
        accepted.fetch_add(1);
        cv.notify_all();
    });

    auto adopted = acc.adopt_native_handle(static_cast<int>(native), /*ipv6=*/false);
    REQUIRE(adopted.has_value());
    REQUIRE(adopted.value().port() == port);

    rt.run(1);

    // 3. Connect a client twice -- both accepts should be observed.
    auto connect_once = [&]() {
        asio::io_context cctx;
        tcp::socket cli(cctx);
        boost::system::error_code cec;
        cli.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), cec);
        REQUIRE_FALSE(cec);
        // Wait briefly for the server to record the accept.
        cli.close();
    };
    connect_once();
    connect_once();

    {
        std::unique_lock lk{mu};
        REQUIRE(cv.wait_for(lk, std::chrono::seconds(2),
                            [&] { return accepted.load() >= 2; }));
    }

    REQUIRE(accepted.load() >= 2);
    {
        std::scoped_lock lk{mu};
        REQUIRE(peer_ports.size() >= 2);
    }

    acc.close();
    rt.stop();
}
