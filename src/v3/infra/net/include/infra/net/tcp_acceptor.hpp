// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tcp_acceptor.hpp
/// Listens on a TCP endpoint and hands each accepted socket to the
/// caller-supplied `SessionFactory`. The factory typically wraps the
/// socket in a `TcpSession`, attaches handler callbacks, and calls
/// `start()`.
///
/// The acceptor itself owns no session state. It is safe to destroy
/// while sessions remain — their lifetime is governed by their own
/// `shared_ptr` chains.

#include <functional>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include "core/error.hpp"
#include "core/result.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::net {

class TcpSession;

class TcpAcceptor {
public:
    using SessionFactory = std::function<void(std::shared_ptr<TcpSession>)>;

    /// Alternative accept callback that receives the raw Asio
    /// socket. Use this when the caller needs control over the
    /// native handle (e.g. to release it back to a legacy code
    /// path). If set via `set_raw_handler()`, the SessionFactory
    /// is ignored.
    using RawHandler = std::function<void(boost::asio::ip::tcp::socket)>;

    TcpAcceptor(IoRuntime& rt, SessionFactory factory);
    ~TcpAcceptor();

    TcpAcceptor(const TcpAcceptor&)            = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;

    /// Bind+listen on the given endpoint and start accepting.
    /// Returns the bound endpoint (useful when port=0 lets the OS pick).
    core::Result<boost::asio::ip::tcp::endpoint>
    listen(const boost::asio::ip::tcp::endpoint& ep, int backlog = 128);

    /// Convenience overload -- parses `host:port`. `host` may be
    /// `0.0.0.0` / `::` to bind all interfaces, or `127.0.0.1`/`::1`.
    core::Result<boost::asio::ip::tcp::endpoint>
    listen(const std::string& host, std::uint16_t port, int backlog = 128);

    /// Adopt an already-listening native socket handle (e.g. a fd
    /// opened, bound, and `listen()`-ed by legacy code). The caller
    /// retains logical ownership semantics: closing the acceptor will
    /// also close the fd unless the user calls `release_native_handle()`
    /// first. Returns the bound endpoint on success.
    core::Result<boost::asio::ip::tcp::endpoint>
    adopt_native_handle(int fd, bool ipv6 = false);

    /// Release the native handle back to the caller so that closing the
    /// acceptor does not close the fd. Stops accepting. Safe to call
    /// when the acceptor is not open (returns -1).
    ///
    /// @note On Windows, `boost::asio::acceptor::release()` may fail
    ///       with `operation_not_supported` depending on the OS
    ///       version and Boost build configuration. Callers that
    ///       cannot tolerate that should treat a negative return as
    ///       "fd is still owned by the acceptor; closing the acceptor
    ///       will close the fd".
    int release_native_handle();

    /// Install a raw-socket accept callback. Overrides the
    /// SessionFactory passed at construction time when set. May
    /// be called before or after `listen()` / `adopt_native_handle()`.
    void set_raw_handler(RawHandler h) { raw_handler_ = std::move(h); }

    /// Stop accepting (idempotent). Existing sessions are unaffected.
    void close();

    boost::asio::ip::tcp::endpoint local_endpoint() const;

private:
    void do_accept();

    IoRuntime&                       rt_;
    boost::asio::ip::tcp::acceptor   acceptor_;
    SessionFactory                   factory_;
    RawHandler                       raw_handler_;
    bool                             open_ = false;
};

}  // namespace pvpgn::infra::net
