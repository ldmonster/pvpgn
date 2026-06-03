// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/net/io_runtime.hpp"

#include <memory>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>

#if defined(PVPGN_V3_HAVE_FIBER)
#  include <boost/fiber/operations.hpp>
#  include "infra/net/asio_round_robin.hpp"
#endif

namespace pvpgn::infra::net {

IoRuntime::IoRuntime()
    : ctx_(),
      work_guard_(boost::asio::make_work_guard(ctx_)),
      signals_(ctx_) {}

IoRuntime::~IoRuntime() {
    stop();
}

void IoRuntime::run(std::size_t threads, bool install_fiber_scheduler) {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    if (threads == 0) threads = 1;
#if defined(PVPGN_V3_HAVE_FIBER)
    if (install_fiber_scheduler && threads > 1) {
        // round_robin's io_context::service is single-threaded.
        threads = 1;
    }
    if (install_fiber_scheduler) {
        // Aliasing shared_ptr: shares ownership with this IoRuntime
        // (no-op deleter) so round_robin's `shared_ptr<io_context>`
        // contract is satisfied without us giving up ownership.
        std::shared_ptr<boost::asio::io_context> ctx_alias(
            std::shared_ptr<void>{}, &ctx_);
        workers_.reserve(threads);
        workers_.emplace_back([this, ctx_alias]() mutable {
            boost::fibers::use_scheduling_algorithm<
                boost::fibers::asio::round_robin>(ctx_alias);
            ctx_.run();
        });
        return;
    }
#else
    (void)install_fiber_scheduler;
#endif
    workers_.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { ctx_.run(); });
    }
}

void IoRuntime::request_stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    // Signal-safe: this may run on a worker thread (the asio signal handler is
    // dispatched on whichever worker is idle), so it MUST NOT join the workers
    // — that would be a self-join. It only unblocks them: cancel the queued
    // async_wait, drop the work guard, and stop the context so every
    // ctx_.run() returns. Joining happens in wait()/stop() on the owner thread.
    signals_.cancel();
    work_guard_.reset();
    ctx_.stop();
}

void IoRuntime::wait() {
    // Block the calling (owner) thread until the workers finish, which happens
    // once request_stop() has unblocked the io_context. Must not be called from
    // a worker thread. Idempotent: a second call finds no joinable workers.
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
    // Note: IoRuntime is not designed to be restarted after stop().
    // Construct a fresh instance for a new lifecycle.
}

void IoRuntime::stop() {
    // Convenience for owner-thread callers (e.g. the destructor): request the
    // stop, then join. Never call this from a worker thread — use
    // request_stop() there.
    request_stop();
    wait();
}

void IoRuntime::install_signal_handlers(std::initializer_list<int> sigs) {
    signals_.cancel();
    signals_.clear();
    for (int s : sigs) {
        signals_.add(s);
    }
    signals_.async_wait([this](const boost::system::error_code& ec, int /*signo*/) {
        if (ec) return;  // cancelled
        // Runs on a worker thread → request only; the owner thread blocked in
        // wait() then returns and performs the join + graceful shutdown.
        this->request_stop();
    });
}

}  // namespace pvpgn::infra::net
