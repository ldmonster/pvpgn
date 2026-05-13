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

    TcpAcceptor(IoRuntime& rt, SessionFactory factory);
    ~TcpAcceptor();

    TcpAcceptor(const TcpAcceptor&)            = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;

    /// Bind+listen on the given endpoint and start accepting.
    /// Returns the bound endpoint (useful when port=0 lets the OS pick).
    core::Result<boost::asio::ip::tcp::endpoint>
    listen(const boost::asio::ip::tcp::endpoint& ep, int backlog = 128);

    /// Convenience overload — parses `host:port`. `host` may be
    /// `0.0.0.0` / `::` to bind all interfaces, or `127.0.0.1`/`::1`.
    core::Result<boost::asio::ip::tcp::endpoint>
    listen(const std::string& host, std::uint16_t port, int backlog = 128);

    /// Stop accepting (idempotent). Existing sessions are unaffected.
    void close();

    boost::asio::ip::tcp::endpoint local_endpoint() const;

private:
    void do_accept();

    IoRuntime&                       rt_;
    boost::asio::ip::tcp::acceptor   acceptor_;
    SessionFactory                   factory_;
    bool                             open_ = false;
};

}  // namespace pvpgn::infra::net
