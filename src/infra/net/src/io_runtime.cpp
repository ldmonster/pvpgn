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

void IoRuntime::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    // Drop the work guard so run() can return once outstanding handlers
    // complete; signal_set cancellation is needed before ctx_.stop()
    // to avoid leaving the queued async_wait in a stuck state.
    signals_.cancel();
    work_guard_.reset();
    ctx_.stop();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
    // Note: IoRuntime is not designed to be restarted after stop().
    // Construct a fresh instance for a new lifecycle.
}

void IoRuntime::install_signal_handlers(std::initializer_list<int> sigs) {
    signals_.cancel();
    signals_.clear();
    for (int s : sigs) {
        signals_.add(s);
    }
    signals_.async_wait([this](const boost::system::error_code& ec, int /*signo*/) {
        if (ec) return;  // cancelled
        this->stop();
    });
}

}  // namespace pvpgn::infra::net
