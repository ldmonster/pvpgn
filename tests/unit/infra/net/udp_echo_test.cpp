// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <catch2/catch_test_macros.hpp>

#include "infra/net/io_runtime.hpp"
#include "infra/net/udp_endpoint.hpp"

using namespace pvpgn;
namespace asio = boost::asio;
using boost::asio::ip::udp;

TEST_CASE("UdpEndpoint: receive + echo back to sender",
          "[infra][net][udp][integration]") {
    infra::net::IoRuntime rt;

    infra::net::UdpEndpoint server(rt);
    server.set_on_datagram([&](const udp::endpoint& from, core::ByteView v) {
        server.send_to(from, std::vector<std::byte>(v.begin(), v.end()));
    });

    auto bound = server.bind("127.0.0.1", 0);
    REQUIRE(bound.has_value());
    const std::uint16_t port = bound.value().port();
    REQUIRE(port != 0);

    rt.run(1);
    server.start();

    // Sync client.
    asio::io_context cctx;
    udp::socket cli(cctx, udp::endpoint{udp::v4(), 0});

    const std::string payload = "ping";
    udp::endpoint dest{asio::ip::make_address("127.0.0.1"), port};
    cli.send_to(asio::buffer(payload), dest);

    std::array<char, 64> rxbuf{};
    udp::endpoint from;
    cli.non_blocking(false);
    // Bounded wait via socket timeout: poll with a small budget.
    std::size_t n = 0;
    for (int i = 0; i < 200 && n == 0; ++i) {
        boost::system::error_code ec;
        cli.non_blocking(true);
        n = cli.receive_from(asio::buffer(rxbuf.data(), rxbuf.size()),
                             from, 0, ec);
        if (ec && ec != boost::asio::error::would_block) {
            n = 0;
        }
        if (n == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    REQUIRE(n == payload.size());
    REQUIRE(std::string(rxbuf.data(), n) == payload);

    server.close();
    rt.stop();
}

TEST_CASE("UdpEndpoint: invalid bind address surfaces InvalidArgument",
          "[infra][net][udp][error]") {
    infra::net::IoRuntime rt;
    infra::net::UdpEndpoint ep(rt);
    auto r = ep.bind("not-an-address", 0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("UdpEndpoint: adopt_native_handle takes over a pre-bound fd",
          "[infra][net][udp][integration]") {
    // Simulate the legacy bnetd path: open + bind a UDP socket with
    // POSIX APIs, then hand the fd to UdpEndpoint and verify echo.
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    REQUIRE(fd >= 0);
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;  // OS pick
    REQUIRE(::bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == 0);
    socklen_t slen = sizeof(sa);
    REQUIRE(::getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &slen) == 0);
    const std::uint16_t port = ntohs(sa.sin_port);

    infra::net::IoRuntime rt;
    infra::net::UdpEndpoint server(rt);
    server.set_on_datagram([&](const udp::endpoint& from, core::ByteView v) {
        server.send_to(from, std::vector<std::byte>(v.begin(), v.end()));
    });

    auto adopted = server.adopt_native_handle(fd);
    REQUIRE(adopted.has_value());
    REQUIRE(adopted.value().port() == port);

    rt.run(1);
    server.start();

    asio::io_context cctx;
    udp::socket cli(cctx, udp::endpoint{udp::v4(), 0});
    const std::string payload = "adopted";
    udp::endpoint dest{asio::ip::make_address("127.0.0.1"), port};
    cli.send_to(asio::buffer(payload), dest);

    std::array<char, 64> rxbuf{};
    udp::endpoint from;
    std::size_t n = 0;
    for (int i = 0; i < 200 && n == 0; ++i) {
        boost::system::error_code ec;
        cli.non_blocking(true);
        n = cli.receive_from(asio::buffer(rxbuf.data(), rxbuf.size()),
                             from, 0, ec);
        if (ec && ec != boost::asio::error::would_block) n = 0;
        if (n == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(n == payload.size());
    REQUIRE(std::string(rxbuf.data(), n) == payload);

    server.close();
    rt.stop();
}
