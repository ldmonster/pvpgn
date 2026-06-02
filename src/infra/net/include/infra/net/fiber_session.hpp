// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fiber_session.hpp
/// Per-session Boost.Fiber spawn helper. Lets handler code be written in
/// a synchronous, "read-loop" style:
///
/// @code
///   spawn_session(rt, session, [](SessionChannel& chan) {
///       while (auto bytes = chan.recv()) {
///           // ... process bytes ...
///           chan.send(reply);
///       }
///       // peer closed; fiber exits, RAII cleans up.
///   });
/// @endcode
///
/// Bridge contract
/// ---------------
///   * Inbound bytes arrive on Asio worker threads via
///     `TcpSession::on_bytes`. They are pushed into a fiber-aware
///     `buffered_channel`. If the channel is full the bytes are
///     dropped and a counter is incremented (back-pressure must be
///     enforced upstream by sizing the channel correctly for the
///     worst-case latency between recv() calls).
///   * `on_close` closes the channel; the fiber observes that as
///     `recv()` returning `std::nullopt`.
///   * The fiber runs on the IoRuntime's pool. `boost::this_fiber::yield`
///     and `sleep_for` are safe inside the handler.
///
/// Available only when `PVPGN_V3_WITH_FIBER=ON` (then the macro
/// `PVPGN_V3_HAVE_FIBER` is defined PUBLIC on `infra_net`).

#include "infra/net/io_runtime.hpp"

#if defined(PVPGN_V3_HAVE_FIBER)

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <boost/fiber/buffered_channel.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/operations.hpp>

#include "infra/net/fiber.hpp"
#include "infra/net/tcp_session.hpp"

namespace pvpgn::infra::net::fiber {

/// Fiber-side view of one session. Owned by the spawned fiber; the
/// non-fiber side (the Asio bytes/close callbacks) drives this object
/// from worker threads via the internal `buffered_channel`.
class SessionChannel {
public:
    /// @param session The session to write to. Must outlive the channel
    ///   (the spawn helper guarantees this by capturing a `shared_ptr`).
    /// @param capacity Inbound queue capacity in chunks. Must be a
    ///   power of two >= 2 (Boost.Fibers requirement). Note: the
    ///   ring buffer holds `capacity - 1` chunks before pushes start
    ///   dropping.
    /// @param read_timeout Idle-read deadline. If no chunk arrives within
    ///   this duration, `recv()` closes the underlying session and returns
    ///   `std::nullopt` (the handler's read loop exits). Zero (the default)
    ///   means "block forever" — no timeout.
    explicit SessionChannel(std::shared_ptr<TcpSession> session,
                            std::size_t capacity = 64,
                            std::chrono::milliseconds read_timeout =
                                std::chrono::milliseconds::zero())
        : session_(std::move(session)),
          inbox_(capacity),
          read_timeout_(read_timeout) {}

    SessionChannel(const SessionChannel&)            = delete;
    SessionChannel& operator=(const SessionChannel&) = delete;

    /// Block the fiber until either a chunk arrives, the configured
    /// idle-read timeout elapses, or the channel is closed. Returns
    /// nullopt on close *or* timeout. Each chunk is the exact byte
    /// sequence delivered by one `on_bytes` callback (no additional
    /// framing). On timeout the underlying session is closed and
    /// `timed_out()` flips to true.
    std::optional<std::vector<std::byte>> recv() {
        std::vector<std::byte> chunk;
        boost::fibers::channel_op_status status;
        if (read_timeout_ > std::chrono::milliseconds::zero()) {
            status = inbox_.pop_wait_for(chunk, read_timeout_);
            if (status == boost::fibers::channel_op_status::timeout) {
                timed_out_.store(true, std::memory_order_relaxed);
                close();
                return std::nullopt;
            }
        } else {
            status = inbox_.pop(chunk);
        }
        if (status != boost::fibers::channel_op_status::success) {
            return std::nullopt;
        }
        return chunk;
    }

    /// Whether the last `recv()` ended because the idle-read timeout
    /// elapsed (as opposed to a peer/local close).
    bool timed_out() const noexcept {
        return timed_out_.load(std::memory_order_relaxed);
    }

    /// Push bytes onto the session's outbound queue. Thread-safe and
    /// non-blocking; safe to call from the fiber.
    void send(std::vector<std::byte> bytes) {
        if (auto s = session_.lock()) {
            s->send(std::move(bytes));
        }
    }

    /// Initiate session close. The fiber will see `recv()` return
    /// nullopt soon after the close propagates back through Asio.
    void close() {
        if (auto s = session_.lock()) {
            s->close();
        }
    }

    /// Number of inbound chunks dropped because the channel was full
    /// when the network thread tried to push them. Useful for
    /// back-pressure metrics.
    std::size_t dropped() const noexcept { return dropped_.load(); }

    // ---- internal API used by spawn_session ---------------------------

    /// Push bytes from the network thread. Non-blocking. Increments
    /// `dropped_` if the channel is full or already closed.
    void push_from_network(std::vector<std::byte> bytes) {
        const auto s = inbox_.try_push(std::move(bytes));
        if (s != boost::fibers::channel_op_status::success) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    /// Close the inbound channel. Wakes any fiber blocked in `recv`.
    void close_inbox() noexcept {
        inbox_.close();
    }

private:
    std::weak_ptr<TcpSession>                          session_;
    boost::fibers::buffered_channel<std::vector<std::byte>> inbox_;
    std::atomic<std::size_t>                           dropped_{0};
    std::chrono::milliseconds                          read_timeout_;
    std::atomic<bool>                                  timed_out_{false};
};

/// Fiber-style session handler.
using SessionHandler = std::function<void(SessionChannel&)>;

/// Wire @p session's bytes/close callbacks to a fresh `SessionChannel`,
/// then spawn a fiber on @p rt running @p handler. The session is
/// `start()`-ed by this call.
///
/// Lifetime: the channel is heap-allocated and held by both the
/// handler fiber (by reference) and the on_bytes/on_close lambdas (by
/// `shared_ptr`). It dies when both go away, which happens after the
/// fiber returns *and* the session has delivered its last callback.
///
/// **Asio↔Fiber scheduler caveat.** Out of the box, an Asio worker
/// thread does not run fibers between handlers — so a fiber blocked
/// on `SessionChannel::recv()` may not be re-scheduled when bytes
/// arrive on a *different* worker thread. To make `spawn_session`
/// usable in production, the IoRuntime must install
/// `boost::fibers::asio::round_robin` (or equivalent) on each worker.
/// That integration is tracked under refactoring-plan-06; until then
/// `spawn_session` is best used with `IoRuntime::run(1)` so the
/// fiber and the I/O handler run on the same thread.
inline void spawn_session(IoRuntime& rt,
                          std::shared_ptr<TcpSession> session,
                          SessionHandler handler,
                          std::size_t inbox_capacity = 64,
                          std::chrono::milliseconds read_timeout =
                              std::chrono::milliseconds::zero()) {
    auto chan = std::make_shared<SessionChannel>(session, inbox_capacity,
                                                 read_timeout);

    session->set_on_bytes([chan](core::ByteView v) {
        chan->push_from_network(std::vector<std::byte>(v.begin(), v.end()));
    });
    session->set_on_close([chan](const boost::system::error_code&) {
        chan->close_inbox();
    });

    spawn_on(rt, [chan, handler = std::move(handler)]() mutable {
        handler(*chan);
    });

    session->start();
}

}  // namespace pvpgn::infra::net::fiber

#endif  // PVPGN_V3_HAVE_FIBER
