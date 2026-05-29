// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fiber_pool.hpp
/// Multi-threaded fiber-aware worker pool.
///
/// Background
/// ----------
/// `IoRuntime` (see `io_runtime.hpp`) is a single shared `io_context`
/// pumped by N OS threads. That is fine for plain Asio handlers, but
/// the Boost.Fiber `asio::round_robin` scheduler installs a
/// per-`io_context` service and is *single-threaded by design*. Hence
/// the second-arg restriction documented on `IoRuntime::run()`.
///
/// `FiberPool` is the multi-threaded sibling. It owns N independent
/// `io_context`s (one per worker), each with its own `round_robin`.
/// Sessions are pinned to a worker for their entire lifetime — a
/// fiber on worker K only ever runs on the OS thread bound to K.
///
/// Acceptance pattern
/// ------------------
/// A `FiberPool::AcceptHandle` listens on a single TCP endpoint using
/// the *primary* worker's executor (worker 0). On each accepted
/// socket, the pool picks the next worker round-robin, **migrates**
/// the OS socket descriptor to that worker's executor (asio sockets
/// are bound to their executor, so we release+re-attach the native
/// handle), and spawns the fiber-style handler on that worker.
///
/// What you get
/// ------------
///   * `FiberPool::start(threads)`           — spawn N workers.
///   * `FiberPool::stop()`                   — drain & join. RAII.
///   * `FiberPool::next_executor()`          — round-robin pick.
///   * `FiberPool::accept(host, port, hnd)`  — accept-and-spawn.
///
/// What you don't
/// --------------
///   * Cross-worker fiber migration. Once a session is spawned on a
///     worker, its fiber stays there.
///   * Work-stealing. If one worker is hot, we don't redistribute.
///
/// Build gating
/// ------------
/// Available only when `PVPGN_V3_HAVE_FIBER` is defined.

#include "infra/net/fiber.hpp"
#include "infra/net/fiber_session.hpp"

#if defined(PVPGN_V3_HAVE_FIBER)

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::net {

class TcpSession;  // forward, full def in tcp_session.hpp

class FiberPool {
public:
    using SessionHandler = fiber::SessionHandler;

    FiberPool();
    ~FiberPool();

    FiberPool(const FiberPool&)            = delete;
    FiberPool& operator=(const FiberPool&) = delete;
    FiberPool(FiberPool&&)                 = delete;
    FiberPool& operator=(FiberPool&&)      = delete;

    /// Spawn @p threads worker threads, each with its own
    /// `io_context` and round_robin scheduler. Coerced to 1 if 0.
    /// Idempotent — second call is a no-op.
    void start(std::size_t threads);

    /// Stop all workers and join. Idempotent. Called automatically by
    /// the destructor.
    void stop();

    /// Round-robin pick of a worker's executor. Useful for hand-built
    /// timers/sockets that should live on the pool.
    boost::asio::any_io_executor next_executor() noexcept;

    /// Number of workers, or 0 before start().
    std::size_t worker_count() const noexcept { return workers_.size(); }

    /// Begin accepting on @p host : @p port. The acceptor lives on
    /// worker 0; each accepted socket is moved to a round-robin
    /// chosen worker, wrapped in a TcpSession, and dispatched via
    /// `spawn_session(...)` on that worker.
    ///
    /// @return The bound endpoint or an Error.
    core::Result<boost::asio::ip::tcp::endpoint, core::Error>
    accept(const std::string& host,
           std::uint16_t      port,
           SessionHandler     handler,
           std::size_t        inbox_capacity = 64);

    /// Stop accepting (closes the acceptor). Existing sessions
    /// continue to run until their handlers return.
    void close_acceptor() noexcept;

private:
    struct Worker {
        boost::asio::io_context                                       ctx;
        boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type>                   work_guard;
        std::thread                                                   thread;

        Worker() : work_guard(boost::asio::make_work_guard(ctx)) {}
    };

    std::vector<std::unique_ptr<Worker>>          workers_;
    std::atomic<std::size_t>                      next_worker_{0};
    std::atomic<bool>                             running_{false};

    // Acceptor lives on worker 0 when active.
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;

    Worker& pick_worker_round_robin() noexcept;
};

}  // namespace pvpgn::infra::net

#endif  // PVPGN_V3_HAVE_FIBER
