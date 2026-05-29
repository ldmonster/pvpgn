// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/net/udp_endpoint.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/post.hpp>

namespace pvpgn::infra::net {

namespace asio = boost::asio;
using boost::system::error_code;

UdpEndpoint::UdpEndpoint(IoRuntime& rt)
    : rt_(rt),
      socket_(rt.context()),
      strand_(asio::make_strand(rt.context().get_executor())) {}

UdpEndpoint::~UdpEndpoint() { close(); }

core::Result<asio::ip::udp::endpoint>
UdpEndpoint::bind(const asio::ip::udp::endpoint& ep) {
    error_code ec;
    socket_.open(ep.protocol(), ec);
    if (ec) return core::fail(core::Error{core::StatusCode::Internal,
                               "udp socket open failed: " + ec.message()});
    socket_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) return core::fail(core::Error{core::StatusCode::Internal,
                               "udp set reuse_address: " + ec.message()});
    socket_.bind(ep, ec);
    if (ec) return core::fail(core::Error{core::StatusCode::InvalidArgument,
                               "udp bind " + ep.address().to_string()
                               + ": " + ec.message()});
    closed_ = false;
    return socket_.local_endpoint();
}

core::Result<asio::ip::udp::endpoint>
UdpEndpoint::bind(const std::string& host, std::uint16_t port) {
    error_code ec;
    auto addr = asio::ip::make_address(host, ec);
    if (ec) return core::fail(core::Error{core::StatusCode::InvalidArgument,
                               "invalid bind address: " + host});
    return bind(asio::ip::udp::endpoint{addr, port});
}

core::Result<asio::ip::udp::endpoint>
UdpEndpoint::adopt_native_handle(int fd, bool ipv6) {
    error_code ec;
    if (socket_.is_open()) {
        socket_.close(ec);
    }
    auto proto = ipv6 ? asio::ip::udp::v6() : asio::ip::udp::v4();
    socket_.assign(proto, fd, ec);
    if (ec) return core::fail(core::Error{core::StatusCode::Internal,
                               "udp adopt fd failed: " + ec.message()});
    socket_.non_blocking(true, ec);  // ec ignored: legacy may have set it
    closed_ = false;
    auto local = socket_.local_endpoint(ec);
    if (ec) return core::fail(core::Error{core::StatusCode::Internal,
                               "udp adopt: local_endpoint failed: " + ec.message()});
    return local;
}

void UdpEndpoint::start() {
    asio::post(strand_, [this] {
        if (closed_ || receiving_) return;
        receiving_ = true;
        do_receive();
    });
}

void UdpEndpoint::do_receive() {
    socket_.async_receive_from(
        asio::buffer(rx_buf_.data(), rx_buf_.size()),
        rx_from_,
        asio::bind_executor(strand_,
            [this](const error_code& ec, std::size_t n) {
                if (ec) {
                    receiving_ = false;
                    if (closed_) return;
                    if (on_error_) on_error_(ec);
                    return;
                }
                if (n > 0 && on_datagram_) {
                    on_datagram_(rx_from_,
                                 core::ByteView{rx_buf_.data(), n});
                }
                if (closed_) {
                    receiving_ = false;
                    return;
                }
                do_receive();
            }));
}

void UdpEndpoint::send_to(asio::ip::udp::endpoint remote,
                          std::vector<std::byte> bytes) {
    asio::post(strand_, [this, r = std::move(remote),
                               b = std::move(bytes)]() mutable {
        std::scoped_lock lk{mu_};
        if (closed_) return;
        tx_q_.push_back(OutItem{std::move(r), std::move(b)});
        if (!sending_) {
            sending_ = true;
            do_send_locked();
        }
    });
}

void UdpEndpoint::do_send_locked() {
    // Caller holds mu_. The front item is owned by the deque; safe to
    // pass its buffer by reference to async_send_to.
    auto& front = tx_q_.front();
    socket_.async_send_to(
        asio::buffer(front.bytes.data(), front.bytes.size()),
        front.to,
        asio::bind_executor(strand_,
            [this](const error_code& ec, std::size_t /*n*/) {
                std::scoped_lock lk{mu_};
                tx_q_.pop_front();
                if (ec) {
                    sending_ = false;
                    if (on_error_) on_error_(ec);
                    return;
                }
                if (closed_ || tx_q_.empty()) {
                    sending_ = false;
                    return;
                }
                do_send_locked();
            }));
}

void UdpEndpoint::close() {
    asio::post(strand_, [this] {
        if (closed_) return;
        closed_ = true;
        error_code ignored;
        socket_.close(ignored);
    });
}

asio::ip::udp::endpoint UdpEndpoint::local_endpoint() const {
    error_code ec;
    return socket_.local_endpoint(ec);
}

}  // namespace pvpgn::infra::net
