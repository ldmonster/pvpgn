// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file io_runtime.hpp
/// Asio-backed worker pool. Owns one `io_context` and N worker threads;
/// the new networking layer schedules every operation onto it.
///
/// Design notes
/// ------------
///  * One global runtime per process (DI-injected; not a singleton).
///  * `run(threads)` spawns workers and returns; `stop()` halts them
///    and joins. Destructor enforces stop()+join() to keep RAII tidy.
///  * `executor()` returns an Asio executor handle; layers above pass
///    it to `tcp::socket`/`tcp::acceptor`/`steady_timer`/etc.
///  * `install_signal_handlers({SIGINT,SIGTERM})` wires graceful stop
///    via `asio::signal_set` — the legacy `extern volatile sig_atomic_t`
///    pattern goes away with the last legacy caller.
///
/// Thread-safety: `post()` and `executor()` are safe from any thread.
/// `run()`/`stop()` are *not* meant to be called concurrently with
/// themselves.

#include <atomic>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/signal_set.hpp>

namespace pvpgn::infra::net {

class IoRuntime {
public:
    using executor_type = boost::asio::io_context::executor_type;

    IoRuntime();
    ~IoRuntime();

    IoRuntime(const IoRuntime&)            = delete;
    IoRuntime& operator=(const IoRuntime&) = delete;
    IoRuntime(IoRuntime&&)                 = delete;
    IoRuntime& operator=(IoRuntime&&)      = delete;

    /// Spawn @p threads worker threads. Returns immediately. If already
    /// running, this is a no-op.
    ///
    /// @param threads Number of worker threads. Coerced to 1 if 0.
    /// @param install_fiber_scheduler When true (and the build has
    ///   `PVPGN_V3_HAVE_FIBER` defined), each worker thread installs
    ///   `boost::fibers::asio::round_robin` as its scheduler so
    ///   fibers spawned via `infra::net::fiber::spawn_on` /
    ///   `spawn_session` are co-operatively scheduled with Asio
    ///   handlers. When false (default) the scheduler is the plain
    ///   round-robin Asio loop and fibers will NOT progress unless the
    ///   handler thread happens to yield.
    ///
    ///   ⚠ The Boost.Fiber `asio::round_robin` scheduler installs an
    ///   `io_context::service` per io_context, so it must run on a
    ///   single worker thread. Calling with `install_fiber_scheduler
    ///   == true && threads > 1` is a programming error and is
    ///   coerced down to `threads = 1`.
    void run(std::size_t threads = 1,
             bool        install_fiber_scheduler = false);

    /// Cooperatively stop workers and join them. Idempotent. Call only from
    /// the owner thread (never a worker) — it joins, which would self-join.
    void stop();

    /// Request a cooperative stop WITHOUT joining. Signal-safe: may be called
    /// from a worker thread (e.g. the asio signal handler). Unblocks every
    /// worker's `io_context::run()`; pair with wait() on the owner thread.
    void request_stop();

    /// Block the calling (owner) thread until all workers have finished
    /// (i.e. until request_stop()/stop() has unblocked the io_context). This
    /// is what keeps a daemon's main thread alive after run() spawns the
    /// workers. Must not be called from a worker thread.
    void wait();

    /// Schedule @p fn for execution on a worker thread.
    template <class F>
    void post(F&& fn) {
        boost::asio::post(ctx_, std::forward<F>(fn));
    }

    /// Return an Asio executor handle; pass to sockets/timers.
    executor_type executor() noexcept { return ctx_.get_executor(); }

    /// Raw `io_context` access — escape hatch for code that needs an
    /// `asio::*::*::executor_type`-typed object. Prefer `executor()`.
    boost::asio::io_context& context() noexcept { return ctx_; }

    /// Wire graceful shutdown for the listed POSIX signals. The first
    /// matching signal triggers `stop()` and the handler dropper. May
    /// be called multiple times; later calls replace the previous set.
    void install_signal_handlers(std::initializer_list<int> signals);

    bool running() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    boost::asio::io_context                                   ctx_;
    boost::asio::executor_work_guard<executor_type>           work_guard_;
    boost::asio::signal_set                                   signals_;
    std::vector<std::thread>                                  workers_;
    std::atomic<bool>                                         running_{false};
};

}  // namespace pvpgn::infra::net
