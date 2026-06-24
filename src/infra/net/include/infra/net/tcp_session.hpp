// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tcp_session.hpp
/// Owns one accepted TCP socket and pumps bytes through a user-supplied
/// handler. Lifetime is `shared_ptr` because Asio callbacks keep a
/// reference until the operation completes.
///
/// Handler contract
/// ----------------
///   * `on_bytes(ByteView)` is called from a worker thread whenever
///     new bytes arrive. The view is valid only during the call.
///   * `on_close(error_code)` is called exactly once when the session
///     ends (peer close, transport error, or `close()` from the app).
///
/// Both callbacks are optional; if unset they default to no-ops.
///
/// Writes are queued in a `std::deque<std::vector<std::byte>>` and
/// serialised via a single in-flight `async_write`.

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include "core/bytes.hpp"

namespace pvpgn::infra::net {

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    using OnBytes = std::function<void(core::ByteView)>;
    using OnClose = std::function<void(const boost::system::error_code&)>;

    static std::shared_ptr<TcpSession> create(boost::asio::ip::tcp::socket sock) {
        return std::shared_ptr<TcpSession>(new TcpSession(std::move(sock)));
    }

    void set_on_bytes(OnBytes cb) { on_bytes_ = std::move(cb); }
    void set_on_close(OnClose cb) { on_close_ = std::move(cb); }

    /// Set the idle-read deadline. If no bytes are received within this
    /// duration the session closes itself (delivering `on_close` with
    /// `boost::asio::error::timed_out`). Zero (the default) disables the
    /// timeout. Must be called before `start()`.
    void set_idle_timeout(std::chrono::milliseconds d) noexcept { idle_timeout_ = d; }

    /// Begin reading. Idempotent: a second call after `close()` is a no-op.
    void start();

    /// Enqueue bytes for transmission. Safe from any thread.
    void send(std::vector<std::byte> bytes);

    /// Initiate graceful shutdown. Idempotent.
    void close();

    boost::asio::ip::tcp::endpoint remote_endpoint() const;

    /// Returns the OS-level socket descriptor as a signed integer.
    /// Used to build a legacy `t_connection` that shares the fd
    /// with this Asio socket.
    /// Asio retains ownership: the caller MUST NOT close the
    /// returned fd. Returns `-1` if the socket has been closed.
    int native_handle_int() noexcept;

private:
    explicit TcpSession(boost::asio::ip::tcp::socket sock);

    void do_read();
    void do_write_locked();
    void deliver_close(const boost::system::error_code& ec);
    /// (Re)arm the idle-read deadline. No-op when the timeout is disabled.
    /// Must run on `strand_`.
    void arm_idle_timer();

    boost::asio::ip::tcp::socket                                socket_;
    boost::asio::strand<boost::asio::any_io_executor>           strand_;
    boost::asio::steady_timer                                   idle_timer_;
    std::chrono::milliseconds                                   idle_timeout_{0};
    std::array<std::byte, 4096>                                 read_buf_{};
    std::deque<std::vector<std::byte>>                          write_q_;
    bool                                                        writing_ = false;
    bool                                                        closed_  = false;
    std::mutex                                                  mu_;
    OnBytes                                                     on_bytes_;
    OnClose                                                     on_close_;
};

}  // namespace pvpgn::infra::net
