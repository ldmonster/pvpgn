// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/net/tcp_session.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/post.hpp>

namespace pvpgn::infra::net {

namespace asio = boost::asio;
using boost::system::error_code;

TcpSession::TcpSession(asio::ip::tcp::socket sock)
    : socket_(std::move(sock)),
      strand_(asio::make_strand(socket_.get_executor())) {}

void TcpSession::start() {
    auto self = shared_from_this();
    asio::post(strand_, [self] { self->do_read(); });
}

asio::ip::tcp::endpoint TcpSession::remote_endpoint() const {
    error_code ec;
    return socket_.remote_endpoint(ec);
}

int TcpSession::native_handle_int() noexcept {
    if (!socket_.is_open()) {
        return -1;
    }
    return static_cast<int>(socket_.native_handle());
}

void TcpSession::do_read() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buf_.data(), read_buf_.size()),
        asio::bind_executor(strand_, [self](const error_code& ec, std::size_t n) {
            if (ec) {
                self->deliver_close(ec);
                return;
            }
            if (n > 0 && self->on_bytes_) {
                self->on_bytes_(core::ByteView{self->read_buf_.data(), n});
            }
            if (self->closed_) return;
            self->do_read();
        }));
}

void TcpSession::send(std::vector<std::byte> bytes) {
    auto self = shared_from_this();
    asio::post(strand_, [self, b = std::move(bytes)]() mutable {
        std::scoped_lock lk{self->mu_};
        if (self->closed_) return;
        self->write_q_.push_back(std::move(b));
        if (!self->writing_) {
            self->writing_ = true;
            self->do_write_locked();
        }
    });
}

void TcpSession::do_write_locked() {
    // Caller holds mu_. We grab the head buffer by reference; it stays
    // alive in the deque until the write completes.
    auto& front = write_q_.front();
    auto self = shared_from_this();
    asio::async_write(
        socket_, asio::buffer(front.data(), front.size()),
        asio::bind_executor(strand_, [self](const error_code& ec, std::size_t /*n*/) {
            std::scoped_lock lk{self->mu_};
            self->write_q_.pop_front();
            if (ec) {
                self->writing_ = false;
                // Schedule close outside the lock.
                asio::post(self->strand_, [self, ec] { self->deliver_close(ec); });
                return;
            }
            if (self->closed_ || self->write_q_.empty()) {
                self->writing_ = false;
                return;
            }
            self->do_write_locked();
        }));
}

void TcpSession::close() {
    auto self = shared_from_this();
    asio::post(strand_, [self] {
        if (self->closed_) return;
        self->closed_ = true;
        error_code ignored;
        self->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        self->socket_.close(ignored);
        if (self->on_close_) self->on_close_(error_code{});
    });
}

void TcpSession::deliver_close(const error_code& ec) {
    if (closed_) return;
    closed_ = true;
    error_code ignored;
    socket_.close(ignored);
    if (on_close_) on_close_(ec);
}

}  // namespace pvpgn::infra::net
