// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/net/tcp_acceptor.hpp"

#include <boost/asio/ip/address.hpp>

#include "infra/net/tcp_session.hpp"

namespace pvpgn::infra::net {

namespace asio = boost::asio;
using boost::system::error_code;

TcpAcceptor::TcpAcceptor(IoRuntime& rt, SessionFactory factory)
    : rt_(rt),
      acceptor_(rt.context()),
      factory_(std::move(factory)) {}

TcpAcceptor::~TcpAcceptor() { close(); }

core::Result<asio::ip::tcp::endpoint>
TcpAcceptor::listen(const asio::ip::tcp::endpoint& ep, int backlog) {
    error_code ec;
    acceptor_.open(ep.protocol(), ec);
    if (ec) return core::fail(core::Error{core::StatusCode::Internal,
                               "tcp acceptor open failed: " + ec.message()});
    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) return core::fail(core::Error{core::StatusCode::Internal,
                               "set reuse_address: " + ec.message()});
    acceptor_.bind(ep, ec);
    if (ec) return core::fail(core::Error{core::StatusCode::InvalidArgument,
                               "bind " + ep.address().to_string() + ": " + ec.message()});
    acceptor_.listen(backlog, ec);
    if (ec) return core::fail(core::Error{core::StatusCode::Internal,
                               "listen: " + ec.message()});
    open_ = true;
    do_accept();
    return acceptor_.local_endpoint();
}

core::Result<asio::ip::tcp::endpoint>
TcpAcceptor::listen(const std::string& host, std::uint16_t port, int backlog) {
    error_code ec;
    auto addr = asio::ip::make_address(host, ec);
    if (ec) return core::fail(core::Error{core::StatusCode::InvalidArgument,
                               "invalid bind address: " + host});
    return listen(asio::ip::tcp::endpoint{addr, port}, backlog);
}

void TcpAcceptor::close() {
    if (!open_) return;
    open_ = false;
    error_code ignored;
    acceptor_.close(ignored);
}

asio::ip::tcp::endpoint TcpAcceptor::local_endpoint() const {
    error_code ec;
    return acceptor_.local_endpoint(ec);
}

void TcpAcceptor::do_accept() {
    acceptor_.async_accept([this](const error_code& ec, asio::ip::tcp::socket sock) {
        if (ec) {
            // Acceptor closed or transport error: stop the loop.
            return;
        }
        if (factory_) {
            auto session = TcpSession::create(std::move(sock));
            factory_(session);
        }
        if (open_) do_accept();
    });
}

}  // namespace pvpgn::infra::net
