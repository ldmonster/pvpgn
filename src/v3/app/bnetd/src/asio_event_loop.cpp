// SPDX-License-Identifier: GPL-2.0-or-later

/// @file asio_event_loop.cpp
/// Implementation of `AsioEventLoop`.

#include "app/bnetd/asio_event_loop.hpp"

#include <boost/asio/post.hpp>

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

AsioEventLoop::AsioEventLoop()
    : ctx_{}
    , work_guard_{boost::asio::make_work_guard(ctx_.get_executor())}
{}

AsioEventLoop::~AsioEventLoop() {
    // Ensure the context is stopped and drained before destruction.
    stop();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AsioEventLoop::run() {
    ctx_.run();
}

void AsioEventLoop::run_for(std::chrono::milliseconds budget) {
    // Temporarily release the work guard so that io_context::run_for()
    // can return when there is no pending work within the budget window.
    // We reset the guard afterwards so that subsequent calls (and the
    // full run() path) still keep the context alive.
    work_guard_.reset();
    ctx_.run_for(budget);
    // Restart the context if it was stopped by run_for() exhausting work.
    ctx_.restart();
    // Re-install the work guard so the context stays alive for future calls.
    work_guard_.emplace(boost::asio::make_work_guard(ctx_.get_executor()));
}

void AsioEventLoop::stop() {
    // Release the work guard first so that run() can return once the
    // current handler (if any) finishes.
    work_guard_.reset();
    ctx_.stop();
}

// ---------------------------------------------------------------------------
// Scheduling
// ---------------------------------------------------------------------------

void AsioEventLoop::post(std::function<void()> fn) {
    boost::asio::post(ctx_, std::move(fn));
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

boost::asio::io_context& AsioEventLoop::io_context() noexcept {
    return ctx_;
}

}  // namespace pvpgn::app::bnetd
