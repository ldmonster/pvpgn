// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for infra/net/socket.hpp
//
// Tests cover:
//   UniqueSocket:
//     - Default construction yields invalid socket
//     - Move construction transfers fd; source becomes invalid
//     - Move assignment transfers fd; source becomes invalid
//     - release() returns fd; socket becomes invalid
//     - reset() closes old fd and takes new one
//     - reset() with no argument leaves socket invalid
//   IpAddress:
//     - from_string("127.0.0.1") succeeds
//     - from_string("invalid") returns nullopt
//     - from_string("") returns nullopt
//     - any() is valid (0.0.0.0)
//     - loopback() is valid (127.0.0.1)
//     - to_string() round-trips
//     - host_order() / network_order() byte-swap correctly
//     - operator== / operator!=
//   Port:
//     - host_order() returns the value passed to constructor
//     - network_order() byte-swaps correctly
//     - operator== / operator!=
//   SocketAddress:
//     - from(IpAddress, Port) constructs correctly
//     - ip() and port() round-trip
//   Factory functions (integration — tagged [integration]):
//     - make_tcp_socket() returns valid socket
//     - make_udp_socket() returns valid socket
//     - set_reuse_addr() succeeds on a valid socket
//     - set_nonblocking() succeeds on a valid socket
//     - bind_socket() succeeds on loopback:0
//     - listen_socket() succeeds after bind
//     - accept_connection() returns nullopt when no peer (non-blocking)
//   WinsockGuard:
//     - initialized() returns true (POSIX always true; Win32 after WSAStartup)

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "infra/net/socket.hpp"

namespace net = pvpgn::infra::net;

// ---------------------------------------------------------------------------
// UniqueSocket — unit tests (no real sockets needed)
// ---------------------------------------------------------------------------

TEST_CASE("UniqueSocket: default construction yields invalid socket",
          "[infra][net][socket][unit]") {
    net::UniqueSocket s;
    CHECK_FALSE(s.valid());
    CHECK(s.get() == net::kInvalidSocket);
}

TEST_CASE("UniqueSocket: explicit construction with kInvalidSocket is invalid",
          "[infra][net][socket][unit]") {
    net::UniqueSocket s{net::kInvalidSocket};
    CHECK_FALSE(s.valid());
}

TEST_CASE("UniqueSocket: move construction transfers fd; source becomes invalid",
          "[infra][net][socket][unit]") {
    // Use a real fd so we can verify transfer without closing a fake one.
    // We open a pipe fd on POSIX; on Windows we use a real socket.
#ifndef _WIN32
    int fds[2] = {-1, -1};
    REQUIRE(::pipe(fds) == 0);
    net::UniqueSocket a{fds[0]};  // owns fds[0]
    REQUIRE(a.valid());

    net::UniqueSocket b{std::move(a)};
    CHECK(b.valid());
    CHECK(b.get() == fds[0]);
    CHECK_FALSE(a.valid());
    CHECK(a.get() == net::kInvalidSocket);

    // Close fds[1] manually (b owns fds[0] and will close it on destruction).
    ::close(fds[1]);
#else
    // On Windows just test with kInvalidSocket — the logic is the same.
    net::UniqueSocket a{net::kInvalidSocket};
    net::UniqueSocket b{std::move(a)};
    CHECK_FALSE(b.valid());
    CHECK_FALSE(a.valid());
#endif
}

TEST_CASE("UniqueSocket: move assignment transfers fd; source becomes invalid",
          "[infra][net][socket][unit]") {
#ifndef _WIN32
    int fds[2] = {-1, -1};
    REQUIRE(::pipe(fds) == 0);
    net::UniqueSocket a{fds[0]};
    net::UniqueSocket b;

    b = std::move(a);
    CHECK(b.valid());
    CHECK(b.get() == fds[0]);
    CHECK_FALSE(a.valid());

    ::close(fds[1]);
#else
    net::UniqueSocket a{net::kInvalidSocket};
    net::UniqueSocket b;
    b = std::move(a);
    CHECK_FALSE(b.valid());
    CHECK_FALSE(a.valid());
#endif
}

TEST_CASE("UniqueSocket: release() returns fd; socket becomes invalid",
          "[infra][net][socket][unit]") {
#ifndef _WIN32
    int fds[2] = {-1, -1};
    REQUIRE(::pipe(fds) == 0);
    net::UniqueSocket s{fds[0]};
    REQUIRE(s.valid());

    net::SocketFd released = s.release();
    CHECK(released == fds[0]);
    CHECK_FALSE(s.valid());

    // We now own fds[0] — close both ends.
    ::close(released);
    ::close(fds[1]);
#else
    net::UniqueSocket s{net::kInvalidSocket};
    net::SocketFd released = s.release();
    CHECK(released == net::kInvalidSocket);
    CHECK_FALSE(s.valid());
#endif
}

TEST_CASE("UniqueSocket: reset() with no argument leaves socket invalid",
          "[infra][net][socket][unit]") {
    net::UniqueSocket s;
    s.reset();
    CHECK_FALSE(s.valid());
}

TEST_CASE("UniqueSocket: reset(fd) takes ownership of new fd",
          "[infra][net][socket][unit]") {
#ifndef _WIN32
    int fds[2] = {-1, -1};
    REQUIRE(::pipe(fds) == 0);
    net::UniqueSocket s;
    s.reset(fds[0]);
    CHECK(s.valid());
    CHECK(s.get() == fds[0]);
    // s destructor closes fds[0]; close fds[1] manually.
    ::close(fds[1]);
#else
    net::UniqueSocket s;
    s.reset(net::kInvalidSocket);
    CHECK_FALSE(s.valid());
#endif
}

// ---------------------------------------------------------------------------
// IpAddress — unit tests
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: from_string(\"127.0.0.1\") succeeds",
          "[infra][net][socket][unit]") {
    auto addr = net::IpAddress::from_string("127.0.0.1");
    REQUIRE(addr.has_value());
    CHECK(addr->to_string() == "127.0.0.1");
}

TEST_CASE("IpAddress: from_string(\"invalid\") returns nullopt",
          "[infra][net][socket][unit]") {
    auto addr = net::IpAddress::from_string("invalid");
    CHECK_FALSE(addr.has_value());
}

TEST_CASE("IpAddress: from_string(\"\") returns nullopt",
          "[infra][net][socket][unit]") {
    auto addr = net::IpAddress::from_string("");
    CHECK_FALSE(addr.has_value());
}

TEST_CASE("IpAddress: from_string(\"999.999.999.999\") returns nullopt",
          "[infra][net][socket][unit]") {
    auto addr = net::IpAddress::from_string("999.999.999.999");
    CHECK_FALSE(addr.has_value());
}

TEST_CASE("IpAddress: any() is 0.0.0.0",
          "[infra][net][socket][unit]") {
    auto addr = net::IpAddress::any();
    CHECK(addr.to_string() == "0.0.0.0");
    CHECK(addr.host_order() == 0u);
}

TEST_CASE("IpAddress: loopback() is 127.0.0.1",
          "[infra][net][socket][unit]") {
    auto addr = net::IpAddress::loopback();
    CHECK(addr.to_string() == "127.0.0.1");
    CHECK(addr.host_order() == 0x7F000001u);
}

TEST_CASE("IpAddress: to_string() round-trips",
          "[infra][net][socket][unit]") {
    const std::string ip_str = "192.168.1.100";
    auto addr = net::IpAddress::from_string(ip_str);
    REQUIRE(addr.has_value());
    CHECK(addr->to_string() == ip_str);
}

TEST_CASE("IpAddress: host_order() and network_order() are byte-swapped",
          "[infra][net][socket][unit]") {
    // 1.2.3.4 in host order = 0x01020304
    auto addr = net::IpAddress::from_string("1.2.3.4");
    REQUIRE(addr.has_value());
    CHECK(addr->host_order() == 0x01020304u);
    CHECK(addr->network_order() == htonl(0x01020304u));
}

TEST_CASE("IpAddress: operator== and operator!=",
          "[infra][net][socket][unit]") {
    auto a = net::IpAddress::loopback();
    auto b = net::IpAddress::loopback();
    auto c = net::IpAddress::any();
    CHECK(a == b);
    CHECK_FALSE(a == c);
    CHECK(a != c);
}

// ---------------------------------------------------------------------------
// Port — unit tests
// ---------------------------------------------------------------------------

TEST_CASE("Port: host_order() returns the value passed to constructor",
          "[infra][net][socket][unit]") {
    net::Port p{8080};
    CHECK(p.host_order() == 8080u);
}

TEST_CASE("Port: network_order() byte-swaps correctly",
          "[infra][net][socket][unit]") {
    net::Port p{8080};
    CHECK(p.network_order() == htons(8080));
}

TEST_CASE("Port: default construction is port 0",
          "[infra][net][socket][unit]") {
    net::Port p;
    CHECK(p.host_order() == 0u);
}

TEST_CASE("Port: operator== and operator!=",
          "[infra][net][socket][unit]") {
    net::Port a{80};
    net::Port b{80};
    net::Port c{443};
    CHECK(a == b);
    CHECK_FALSE(a == c);
    CHECK(a != c);
}

// ---------------------------------------------------------------------------
// SocketAddress — unit tests
// ---------------------------------------------------------------------------

TEST_CASE("SocketAddress: from(IpAddress, Port) constructs correctly",
          "[infra][net][socket][unit]") {
    auto ip   = net::IpAddress::loopback();
    auto port = net::Port{9000};
    auto sa   = net::SocketAddress::from(ip, port);

    CHECK(sa.ip() == ip);
    CHECK(sa.port() == port);
    CHECK(sa.addr.sin_family == AF_INET);
    CHECK(sa.addr.sin_port == htons(9000));
}

TEST_CASE("SocketAddress: ip() and port() round-trip",
          "[infra][net][socket][unit]") {
    auto ip   = net::IpAddress::from_string("10.0.0.1");
    REQUIRE(ip.has_value());
    auto port = net::Port{12345};
    auto sa   = net::SocketAddress::from(*ip, port);

    CHECK(sa.ip().to_string() == "10.0.0.1");
    CHECK(sa.port().host_order() == 12345u);
}

TEST_CASE("SocketAddress: size() matches sizeof(sockaddr_in)",
          "[infra][net][socket][unit]") {
    CHECK(net::SocketAddress::size() == static_cast<socklen_t>(sizeof(sockaddr_in)));
}

// ---------------------------------------------------------------------------
// WinsockGuard — unit test
// ---------------------------------------------------------------------------

TEST_CASE("WinsockGuard: initialized() returns true",
          "[infra][net][socket][unit]") {
    net::WinsockGuard guard;
    CHECK(guard.initialized());
}

// ---------------------------------------------------------------------------
// Factory functions — integration tests (require real OS socket support)
// ---------------------------------------------------------------------------

TEST_CASE("make_tcp_socket() returns a valid socket",
          "[infra][net][socket][integration]") {
    net::WinsockGuard wsa;
    REQUIRE(wsa.initialized());

    auto sock = net::make_tcp_socket();
    REQUIRE(sock.has_value());
    CHECK(sock->valid());
}

TEST_CASE("make_udp_socket() returns a valid socket",
          "[infra][net][socket][integration]") {
    net::WinsockGuard wsa;
    REQUIRE(wsa.initialized());

    auto sock = net::make_udp_socket();
    REQUIRE(sock.has_value());
    CHECK(sock->valid());
}

TEST_CASE("set_reuse_addr() succeeds on a valid TCP socket",
          "[infra][net][socket][integration]") {
    net::WinsockGuard wsa;
    REQUIRE(wsa.initialized());

    auto sock = net::make_tcp_socket();
    REQUIRE(sock.has_value());
    CHECK(net::set_reuse_addr(*sock, true));
    CHECK(net::set_reuse_addr(*sock, false));
}

TEST_CASE("set_nonblocking() succeeds on a valid TCP socket",
          "[infra][net][socket][integration]") {
    net::WinsockGuard wsa;
    REQUIRE(wsa.initialized());

    auto sock = net::make_tcp_socket();
    REQUIRE(sock.has_value());
    CHECK(net::set_nonblocking(*sock, true));
    CHECK(net::set_nonblocking(*sock, false));
}

TEST_CASE("bind_socket() succeeds on loopback port 0",
          "[infra][net][socket][integration]") {
    net::WinsockGuard wsa;
    REQUIRE(wsa.initialized());

    auto sock = net::make_tcp_socket();
    REQUIRE(sock.has_value());
    REQUIRE(net::set_reuse_addr(*sock, true));

    auto addr = net::SocketAddress::from(net::IpAddress::loopback(), net::Port{0});
    CHECK(net::bind_socket(*sock, addr));
}

TEST_CASE("listen_socket() succeeds after bind",
          "[infra][net][socket][integration]") {
    net::WinsockGuard wsa;
    REQUIRE(wsa.initialized());

    auto sock = net::make_tcp_socket();
    REQUIRE(sock.has_value());
    REQUIRE(net::set_reuse_addr(*sock, true));

    auto addr = net::SocketAddress::from(net::IpAddress::loopback(), net::Port{0});
    REQUIRE(net::bind_socket(*sock, addr));
    CHECK(net::listen_socket(*sock, 5));
}

TEST_CASE("accept_connection() returns nullopt when no peer (non-blocking)",
          "[infra][net][socket][integration]") {
    net::WinsockGuard wsa;
    REQUIRE(wsa.initialized());

    auto sock = net::make_tcp_socket();
    REQUIRE(sock.has_value());
    REQUIRE(net::set_reuse_addr(*sock, true));
    REQUIRE(net::set_nonblocking(*sock, true));

    auto addr = net::SocketAddress::from(net::IpAddress::loopback(), net::Port{0});
    REQUIRE(net::bind_socket(*sock, addr));
    REQUIRE(net::listen_socket(*sock, 5));

    // No peer has connected — accept should fail immediately (EAGAIN/EWOULDBLOCK).
    auto result = net::accept_connection(*sock);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("socket_error() returns an int (smoke test)",
          "[infra][net][socket][unit]") {
    // Just verify it compiles and returns something.
    int err = net::socket_error();
    (void)err;
    SUCCEED("socket_error() callable");
}

TEST_CASE("socket_error_string() returns a non-empty string for ENOENT",
          "[infra][net][socket][unit]") {
#ifndef _WIN32
    std::string msg = net::socket_error_string(ENOENT);
    CHECK_FALSE(msg.empty());
#else
    std::string msg = net::socket_error_string(WSAENOTSOCK);
    CHECK_FALSE(msg.empty());
#endif
}
