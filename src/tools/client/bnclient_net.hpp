// SPDX-License-Identifier: GPL-2.0-or-later
//
// Portable socket helpers for the v3 client tools (bnchat / bnftp /
// bnbot / bnstat).  Header-only.  Wraps POSIX BSD sockets and
// Winsock2 behind a single thin API so we can drop the legacy
// `compat/psock` shim.
//
// Mirrors the style used by `src/v3/tools/bntrackd/bntrackd.cpp` so
// the two tools share one mental model of "portable socket".

#ifndef PVPGN_V3_CLIENT_BNCLIENT_NET_HPP
#define PVPGN_V3_CLIENT_BNCLIENT_NET_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace pvpgn::client_v3::net {

#ifdef _WIN32
using socket_t          = SOCKET;
using socklen_portable  = int;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
inline int close_socket(socket_t s) noexcept { return ::closesocket(s); }
inline int last_error() noexcept { return ::WSAGetLastError(); }
#else
using socket_t          = int;
using socklen_portable  = socklen_t;
inline constexpr socket_t kInvalidSocket = -1;
inline int close_socket(socket_t s) noexcept { return ::close(s); }
inline int last_error() noexcept { return errno; }
#endif

// Initialise Winsock once per process.  No-op on POSIX.
inline bool sockets_startup() noexcept {
#ifdef _WIN32
    WSADATA wsa{};
    return ::WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

inline void sockets_cleanup() noexcept {
#ifdef _WIN32
    ::WSACleanup();
#endif
}

// Resolve a hostname (IPv4 only — matches the BNet protocol surface)
// into a `sockaddr_in`.  Returns true on success.  `service` is the
// numeric port in host byte order; the caller fills it in.
inline bool resolve_host(std::string_view host,
                         std::uint16_t port,
                         sockaddr_in& out) noexcept {
    std::memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port   = htons(port);

    // Try literal dotted-quad first.
    std::string host_z(host);
    if (::inet_pton(AF_INET, host_z.c_str(), &out.sin_addr) == 1) {
        return true;
    }

    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(host_z.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
        return false;
    }
    auto* a = reinterpret_cast<const sockaddr_in*>(res->ai_addr);
    out.sin_addr = a->sin_addr;
    ::freeaddrinfo(res);
    return true;
}

// Convenience: connect a fresh TCP socket to `host:port`.  On
// failure returns `kInvalidSocket`.  Caller closes via
// `close_socket`.
inline socket_t connect_tcp(std::string_view host, std::uint16_t port) noexcept {
    sockaddr_in saddr{};
    if (!resolve_host(host, port, saddr)) {
        return kInvalidSocket;
    }
    socket_t sd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd == kInvalidSocket) {
        return kInvalidSocket;
    }
    if (::connect(sd, reinterpret_cast<const sockaddr*>(&saddr), sizeof(saddr)) < 0) {
        close_socket(sd);
        return kInvalidSocket;
    }
    return sd;
}

// Bind a fresh UDP socket to the given local port (use 0 for any).
inline socket_t open_udp_local(std::uint16_t port) noexcept {
    socket_t sd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sd == kInvalidSocket) {
        return kInvalidSocket;
    }
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
    if (::bind(sd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket(sd);
        return kInvalidSocket;
    }
    return sd;
}

// Blocking send of exactly `len` bytes.  Returns false on any error
// or short-write / EOF.  Equivalent to legacy
// `psock_send`-with-loop.
inline bool send_all(socket_t sd, const void* data, std::size_t len) noexcept {
    auto* p = static_cast<const char*>(data);
    std::size_t left = len;
    while (left > 0) {
        auto n = ::send(sd, p, static_cast<std::size_t>(left), 0);
        if (n <= 0) {
            return false;
        }
        p    += n;
        left -= static_cast<std::size_t>(n);
    }
    return true;
}

// Blocking recv of exactly `len` bytes.  Returns false on EOF /
// error.  Equivalent to legacy "blockrecv" loop.
inline bool recv_all(socket_t sd, void* data, std::size_t len) noexcept {
    auto* p = static_cast<char*>(data);
    std::size_t left = len;
    while (left > 0) {
        auto n = ::recv(sd, p, static_cast<std::size_t>(left), 0);
        if (n <= 0) {
            return false;
        }
        p    += n;
        left -= static_cast<std::size_t>(n);
    }
    return true;
}

// Format a `sockaddr_in` to a dotted-quad string.  Always nul-
// terminated.  Buffer must be at least INET_ADDRSTRLEN bytes.
inline std::string addr_to_string(const sockaddr_in& a) {
    char buf[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &a.sin_addr, buf, sizeof(buf));
    return std::string{buf};
}

// RAII guard for a socket descriptor.
class socket_holder {
public:
    socket_holder() noexcept = default;
    explicit socket_holder(socket_t s) noexcept : sd_{s} {}
    socket_holder(const socket_holder&)            = delete;
    socket_holder& operator=(const socket_holder&) = delete;
    socket_holder(socket_holder&& other) noexcept : sd_{other.sd_} { other.sd_ = kInvalidSocket; }
    socket_holder& operator=(socket_holder&& other) noexcept {
        if (this != &other) {
            reset();
            sd_       = other.sd_;
            other.sd_ = kInvalidSocket;
        }
        return *this;
    }
    ~socket_holder() { reset(); }

    socket_t get() const noexcept { return sd_; }
    bool     valid() const noexcept { return sd_ != kInvalidSocket; }
    void     reset(socket_t s = kInvalidSocket) noexcept {
        if (sd_ != kInvalidSocket) {
            close_socket(sd_);
        }
        sd_ = s;
    }
    socket_t release() noexcept {
        auto s = sd_;
        sd_    = kInvalidSocket;
        return s;
    }

private:
    socket_t sd_{kInvalidSocket};
};

} // namespace pvpgn::client_v3::net

#endif // PVPGN_V3_CLIENT_BNCLIENT_NET_HPP
