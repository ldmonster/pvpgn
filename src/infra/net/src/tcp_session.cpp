// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/net/tcp_session.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/post.hpp>

namespace pvpgn::infra::net {

namespace asio = boost::asio;
using boost::system::error_code;

TcpSession::TcpSession(asio::ip::tcp::socket sock)
    : socket_(std::move(sock)),
      strand_(asio::make_strand(socket_.get_executor())),
      idle_timer_(strand_) {}

void TcpSession::start() {
    auto self = shared_from_this();
    asio::post(strand_, [self] {
        self->arm_idle_timer();
        self->do_read();
    });
}

void TcpSession::arm_idle_timer() {
    // Runs on strand_. Re-arming cancels the prior async_wait (its handler
    // then sees operation_aborted and bails). Zero == disabled.
    if (idle_timeout_ <= std::chrono::milliseconds::zero()) {
        return;
    }
    auto self = shared_from_this();
    idle_timer_.expires_after(idle_timeout_);
    idle_timer_.async_wait(asio::bind_executor(strand_, [self](const error_code& ec) {
        if (ec == asio::error::operation_aborted) {
            return;  // re-armed by a fresh read, or cancelled at close.
        }
        if (self->closed_) {
            return;
        }
        // No bytes arrived within the deadline: close the idle session.
        self->deliver_close(asio::error::timed_out);
    }));
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
            self->arm_idle_timer();  // bytes seen → reset the idle deadline
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
        self->idle_timer_.cancel();
        error_code ignored;
        self->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        self->socket_.close(ignored);
        self->fire_close_and_release(error_code{});
    });
}

void TcpSession::deliver_close(const error_code& ec) {
    if (closed_) return;
    closed_ = true;
    idle_timer_.cancel();
    error_code ignored;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
    fire_close_and_release(ec);
}

void TcpSession::fire_close_and_release(const error_code& ec) {
    // Fire the close callback, then drop BOTH handlers. The handlers typically
    // capture a shared_ptr to the protocol FSM, which transitively owns this
    // TcpSession through its egress (FSM -> ctx -> egress -> TcpSession). That
    // forms a reference cycle; without clearing the captures here the whole
    // session graph leaks on every disconnect. Move the close handler into a
    // local and clear the members first so the captures stay alive for the
    // duration of the call but are released immediately afterwards. Runs on
    // strand_, so this never races do_read()'s use of on_bytes_.
    auto on_close = std::move(on_close_);
    on_close_ = nullptr;
    on_bytes_ = nullptr;
    if (on_close) on_close(ec);
}

}  // namespace pvpgn::infra::net
