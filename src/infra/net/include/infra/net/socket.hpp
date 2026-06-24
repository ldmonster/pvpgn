// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Thin C++20 RAII wrapper over POSIX sockets / Winsock2.
// No Boost.Asio dependency — uses OS socket APIs directly.
//
// This header lives in infra/net/ (not infra/compat/) because it is a
// socket *abstraction layer*, not merely a portability shim.
//
// ## Types
//
//   `SocketFd`       — platform socket descriptor (int / SOCKET)
//   `IpAddress`      — wraps in_addr; from_string(), to_string(), any(), loopback()
//   `Port`           — strong typedef over uint16_t; host_order(), network_order()
//   `SocketAddress`  — wraps sockaddr_in; ip(), port(), from(IpAddress, Port)
//   `UniqueSocket`   — RAII move-only socket; auto-closes on destruction
//   `WinsockGuard`   — Windows-only RAII WSAStartup/WSACleanup (no-op on POSIX)
//
// ## Factory functions (all [[nodiscard]])
//
//   make_tcp_socket()                          → std::optional<UniqueSocket>
//   make_udp_socket()                          → std::optional<UniqueSocket>
//   bind_socket(UniqueSocket&, SocketAddress)  → bool
//   listen_socket(UniqueSocket&, int backlog)  → bool
//   accept_connection(UniqueSocket&)           → std::optional<std::pair<UniqueSocket, SocketAddress>>
//   connect_socket(UniqueSocket&, SocketAddress) → bool
//   set_nonblocking(UniqueSocket&, bool)       → bool
//   set_reuse_addr(UniqueSocket&, bool)        → bool
//   socket_send(UniqueSocket&, span, flags)    → std::optional<std::size_t>
//   socket_recv(UniqueSocket&, span, flags)    → std::optional<std::size_t>
//   socket_error()                             → int
//   socket_error_string(int)                   → std::string
//
// ## Design decisions
//
//   - No exceptions — errors are reported via std::optional / bool return values.
//   - [[nodiscard]] on all factory / query functions.
//   - Header-only — no .cpp file needed.
//   - Supports POSIX (Linux/macOS) and Win32 (Winsock2).
//   - C++20 std::span used for buffer parameters.
//   - Namespace: pvpgn::infra::net  (matches existing infra/net/ convention)

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <cerrno>
#endif

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>       // C++20
#include <string>
#include <utility>

namespace pvpgn::infra::net {

// ---------------------------------------------------------------------------
// Platform type aliases
// ---------------------------------------------------------------------------

#ifdef _WIN32
using SocketFd = SOCKET;
inline constexpr SocketFd kInvalidSocket = INVALID_SOCKET;
#else
using SocketFd = int;
inline constexpr SocketFd kInvalidSocket = -1;
#endif

// ---------------------------------------------------------------------------
// IpAddress — wraps in_addr
// ---------------------------------------------------------------------------

/// IPv4 address value type.  Stores the address in *network* byte order
/// internally (matching in_addr.s_addr semantics).
struct IpAddress {
    in_addr addr{};

    /// Construct from a dotted-decimal string.  Returns std::nullopt on
    /// failure (invalid string).
    [[nodiscard]] static std::optional<IpAddress> from_string(const std::string& s) noexcept {
        IpAddress result;
#ifdef _WIN32
        int rc = inet_pton(AF_INET, s.c_str(), &result.addr);
#else
        int rc = inet_pton(AF_INET, s.c_str(), &result.addr);
#endif
        if (rc != 1) return std::nullopt;
        return result;
    }

    /// Returns the INADDR_ANY wildcard address (0.0.0.0).
    [[nodiscard]] static IpAddress any() noexcept {
        IpAddress a;
        a.addr.s_addr = htonl(INADDR_ANY);
        return a;
    }

    /// Returns the loopback address (127.0.0.1).
    [[nodiscard]] static IpAddress loopback() noexcept {
        IpAddress a;
        a.addr.s_addr = htonl(INADDR_LOOPBACK);
        return a;
    }

    /// Convert to dotted-decimal string.
    [[nodiscard]] std::string to_string() const noexcept {
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &addr, buf, sizeof(buf));
        return buf;
    }

    /// Raw network-order 32-bit value.
    [[nodiscard]] std::uint32_t network_order() const noexcept {
        return addr.s_addr;
    }

    /// Raw host-order 32-bit value.
    [[nodiscard]] std::uint32_t host_order() const noexcept {
        return ntohl(addr.s_addr);
    }

    bool operator==(const IpAddress& o) const noexcept {
        return addr.s_addr == o.addr.s_addr;
    }
    bool operator!=(const IpAddress& o) const noexcept { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// Port — strong typedef over uint16_t
// ---------------------------------------------------------------------------

/// TCP/UDP port number.  Constructed from a host-order value.
struct Port {
    std::uint16_t value_host{0};  ///< stored in host byte order

    constexpr explicit Port(std::uint16_t host_port = 0) noexcept
        : value_host(host_port) {}

    /// Value in host byte order (as passed to the constructor).
    [[nodiscard]] constexpr std::uint16_t host_order() const noexcept {
        return value_host;
    }

    /// Value in network byte order (for use in sockaddr_in.sin_port).
    [[nodiscard]] std::uint16_t network_order() const noexcept {
        return htons(value_host);
    }

    bool operator==(const Port& o) const noexcept { return value_host == o.value_host; }
    bool operator!=(const Port& o) const noexcept { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// SocketAddress — wraps sockaddr_in
// ---------------------------------------------------------------------------

/// IPv4 socket address (IP + port).
struct SocketAddress {
    sockaddr_in addr{};

    /// Construct from an IpAddress and a Port.
    [[nodiscard]] static SocketAddress from(IpAddress ip, Port port) noexcept {
        SocketAddress sa;
        std::memset(&sa.addr, 0, sizeof(sa.addr));
        sa.addr.sin_family      = AF_INET;
        sa.addr.sin_addr        = ip.addr;
        sa.addr.sin_port        = port.network_order();
        return sa;
    }

    /// Extract the IP address component.
    [[nodiscard]] IpAddress ip() const noexcept {
        IpAddress a;
        a.addr = addr.sin_addr;
        return a;
    }

    /// Extract the port component (host byte order).
    [[nodiscard]] Port port() const noexcept {
        return Port{ntohs(addr.sin_port)};
    }

    /// Raw pointer to the underlying sockaddr (for bind/connect/accept).
    [[nodiscard]] sockaddr* raw() noexcept {
        return reinterpret_cast<sockaddr*>(&addr);
    }
    [[nodiscard]] const sockaddr* raw() const noexcept {
        return reinterpret_cast<const sockaddr*>(&addr);
    }

    [[nodiscard]] static constexpr socklen_t size() noexcept {
        return static_cast<socklen_t>(sizeof(sockaddr_in));
    }
};

// ---------------------------------------------------------------------------
// UniqueSocket — RAII move-only socket
// ---------------------------------------------------------------------------

/// RAII wrapper for a raw socket file descriptor.
///
/// - Move-only (copy is deleted).
/// - Automatically closes the socket on destruction if valid.
/// - `release()` transfers ownership to the caller.
/// - `reset()` closes the current socket and optionally takes a new one.
class UniqueSocket {
public:
    /// Construct an invalid (empty) socket.
    explicit UniqueSocket(SocketFd fd = kInvalidSocket) noexcept : fd_(fd) {}

    /// Destructor — closes the socket if valid.
    ~UniqueSocket() noexcept { close_if_valid(); }

    // Move constructor — transfers ownership.
    UniqueSocket(UniqueSocket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = kInvalidSocket;
    }

    // Move assignment — closes current, takes ownership of other.
    UniqueSocket& operator=(UniqueSocket&& other) noexcept {
        if (this != &other) {
            close_if_valid();
            fd_       = other.fd_;
            other.fd_ = kInvalidSocket;
        }
        return *this;
    }

    // Non-copyable.
    UniqueSocket(const UniqueSocket&)            = delete;
    UniqueSocket& operator=(const UniqueSocket&) = delete;

    /// Returns the underlying file descriptor without releasing ownership.
    [[nodiscard]] SocketFd get() const noexcept { return fd_; }

    /// Returns true if the socket holds a valid descriptor.
    [[nodiscard]] bool valid() const noexcept { return fd_ != kInvalidSocket; }

    /// Releases ownership and returns the raw descriptor.
    /// The caller is responsible for closing it.
    [[nodiscard]] SocketFd release() noexcept {
        SocketFd tmp = fd_;
        fd_          = kInvalidSocket;
        return tmp;
    }

    /// Closes the current socket (if valid) and optionally takes a new one.
    void reset(SocketFd fd = kInvalidSocket) noexcept {
        close_if_valid();
        fd_ = fd;
    }

private:
    SocketFd fd_;

    void close_if_valid() noexcept {
        if (fd_ == kInvalidSocket) return;
#ifdef _WIN32
        ::closesocket(fd_);
#else
        ::close(fd_);
#endif
        fd_ = kInvalidSocket;
    }
};

// ---------------------------------------------------------------------------
// WinsockGuard — Windows-only RAII WSAStartup / WSACleanup
// ---------------------------------------------------------------------------

/// RAII guard that calls WSAStartup on construction and WSACleanup on
/// destruction.  On POSIX systems this is a no-op (zero overhead).
///
/// Typical usage (once per process):
///   WinsockGuard wsa;
///   if (!wsa.initialized()) { /* handle error */ }
class WinsockGuard {
public:
#ifdef _WIN32
    WinsockGuard() noexcept {
        WSADATA data{};
        initialized_ = (::WSAStartup(MAKEWORD(2, 2), &data) == 0);
    }
    ~WinsockGuard() noexcept {
        if (initialized_) ::WSACleanup();
    }
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

    WinsockGuard(WinsockGuard&& other) noexcept
        : initialized_(other.initialized_) {
        other.initialized_ = false;
    }
    WinsockGuard& operator=(WinsockGuard&& other) noexcept {
        if (this != &other) {
            if (initialized_) ::WSACleanup();
            initialized_       = other.initialized_;
            other.initialized_ = false;
        }
        return *this;
    }
    WinsockGuard(const WinsockGuard&)            = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete;

private:
    bool initialized_{false};
#else
    WinsockGuard() noexcept  = default;
    ~WinsockGuard() noexcept = default;
    [[nodiscard]] bool initialized() const noexcept { return true; }

    WinsockGuard(WinsockGuard&&)            = default;
    WinsockGuard& operator=(WinsockGuard&&) = default;
    WinsockGuard(const WinsockGuard&)            = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete;
#endif
};

// ---------------------------------------------------------------------------
// Error helpers
// ---------------------------------------------------------------------------

/// Returns the last socket error code (errno on POSIX, WSAGetLastError on Win32).
[[nodiscard]] inline int socket_error() noexcept {
#ifdef _WIN32
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

/// Returns a human-readable description of a socket error code.
[[nodiscard]] inline std::string socket_error_string(int err) {
#ifdef _WIN32
    char buf[256] = {};
    ::FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(err),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, sizeof(buf), nullptr);
    // Strip trailing newline/CR that FormatMessage appends.
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    return s;
#else
    return std::strerror(err);
#endif
}

// ---------------------------------------------------------------------------
// Factory functions
// ---------------------------------------------------------------------------

/// Create a TCP (SOCK_STREAM) socket.
/// Returns std::nullopt on failure.
[[nodiscard]] inline std::optional<UniqueSocket> make_tcp_socket() noexcept {
    SocketFd fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == kInvalidSocket) return std::nullopt;
    return UniqueSocket{fd};
}

/// Create a UDP (SOCK_DGRAM) socket.
/// Returns std::nullopt on failure.
[[nodiscard]] inline std::optional<UniqueSocket> make_udp_socket() noexcept {
    SocketFd fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == kInvalidSocket) return std::nullopt;
    return UniqueSocket{fd};
}

/// Bind a socket to a local address.
/// Returns true on success.
[[nodiscard]] inline bool bind_socket(UniqueSocket& sock,
                                      SocketAddress addr) noexcept {
    return ::bind(sock.get(), addr.raw(), SocketAddress::size()) == 0;
}

/// Put a socket into the listening state.
/// Returns true on success.
[[nodiscard]] inline bool listen_socket(UniqueSocket& sock,
                                        int backlog = 5) noexcept {
    return ::listen(sock.get(), backlog) == 0;
}

/// Accept an incoming connection.
/// Returns a pair of (new UniqueSocket, remote SocketAddress) on success,
/// or std::nullopt on failure.
[[nodiscard]] inline std::optional<std::pair<UniqueSocket, SocketAddress>>
accept_connection(UniqueSocket& sock) noexcept {
    SocketAddress remote{};
    socklen_t     len = SocketAddress::size();
    SocketFd      fd  = ::accept(sock.get(), remote.raw(), &len);
    if (fd == kInvalidSocket) return std::nullopt;
    return std::make_pair(UniqueSocket{fd}, remote);
}

/// Connect a socket to a remote address.
/// Returns true on success (or EINPROGRESS for non-blocking sockets).
[[nodiscard]] inline bool connect_socket(UniqueSocket& sock,
                                         SocketAddress addr) noexcept {
    int rc = ::connect(sock.get(), addr.raw(), SocketAddress::size());
#ifdef _WIN32
    return rc == 0 || ::WSAGetLastError() == WSAEINPROGRESS
                   || ::WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return rc == 0 || errno == EINPROGRESS;
#endif
}

/// Set or clear the O_NONBLOCK / FIONBIO flag on a socket.
/// Returns true on success.
[[nodiscard]] inline bool set_nonblocking(UniqueSocket& sock,
                                          bool nonblocking) noexcept {
#ifdef _WIN32
    u_long mode = nonblocking ? 1u : 0u;
    return ::ioctlsocket(sock.get(), FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(sock.get(), F_GETFL, 0);
    if (flags < 0) return false;
    if (nonblocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return ::fcntl(sock.get(), F_SETFL, flags) == 0;
#endif
}

/// Set or clear the SO_REUSEADDR socket option.
/// Returns true on success.
[[nodiscard]] inline bool set_reuse_addr(UniqueSocket& sock,
                                         bool reuse) noexcept {
#ifdef _WIN32
    BOOL val = reuse ? TRUE : FALSE;
    return ::setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&val),
                        static_cast<int>(sizeof(val))) == 0;
#else
    int val = reuse ? 1 : 0;
    return ::setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR,
                        &val, static_cast<socklen_t>(sizeof(val))) == 0;
#endif
}

/// Send data from a byte span.
/// Returns the number of bytes sent, or std::nullopt on a fatal error.
/// A return value of 0 is valid (e.g. zero-length span).
[[nodiscard]] inline std::optional<std::size_t>
socket_send(UniqueSocket& sock,
            std::span<const std::byte> buf,
            int flags = 0) noexcept {
#ifdef _WIN32
    int rc = ::send(sock.get(),
                    reinterpret_cast<const char*>(buf.data()),
                    static_cast<int>(buf.size()),
                    flags);
    if (rc == SOCKET_ERROR) return std::nullopt;
#else
    ssize_t rc = ::send(sock.get(),
                        buf.data(),
                        buf.size(),
                        flags);
    if (rc < 0) return std::nullopt;
#endif
    return static_cast<std::size_t>(rc);
}

/// Receive data into a byte span.
/// Returns the number of bytes received (0 = connection closed gracefully),
/// or std::nullopt on a fatal error.
[[nodiscard]] inline std::optional<std::size_t>
socket_recv(UniqueSocket& sock,
            std::span<std::byte> buf,
            int flags = 0) noexcept {
#ifdef _WIN32
    int rc = ::recv(sock.get(),
                    reinterpret_cast<char*>(buf.data()),
                    static_cast<int>(buf.size()),
                    flags);
    if (rc == SOCKET_ERROR) return std::nullopt;
#else
    ssize_t rc = ::recv(sock.get(),
                        buf.data(),
                        buf.size(),
                        flags);
    if (rc < 0) return std::nullopt;
#endif
    return static_cast<std::size_t>(rc);
}

} // namespace pvpgn::infra::net
