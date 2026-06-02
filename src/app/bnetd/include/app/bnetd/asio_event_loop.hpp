// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file asio_event_loop.hpp
/// `AsioEventLoop` — thin, owning wrapper around `boost::asio::io_context`.
///
/// Design
/// ------
/// `AsioEventLoop` owns one `io_context` and an `executor_work_guard` that
/// keeps the context alive even when there are no pending handlers.  The
/// guard is released only when `stop()` is called, which allows `run()` to
/// return naturally.
///
/// Thread-safety
/// -------------
/// * `post()` is safe to call from any thread.
/// * `run()` and `run_for()` must be called from the same thread (or with
///   external synchronisation).
/// * `stop()` is safe to call from any thread.
///
/// Usage in the v3 composition root
/// ---------------------------------
///   AsioEventLoop loop;
///   // ... wire listeners, timers, etc. using loop.io_context() ...
///   loop.run();   // blocks until stop() is called
///
/// Usage for legacy interleaving (LegacyBridge)
/// ---------------------------------------------
///   AsioEventLoop loop;
///   // legacy main loop calls periodically:
///   loop.run_for(std::chrono::milliseconds{10});

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

#include "domain/shared/ports/event_loop.hpp"

namespace pvpgn::app::bnetd {

/// Owns an `asio::io_context` and exposes a minimal lifecycle API.
/// Implements `application::ports::IEventLoop` so it can be injected into
/// use-cases and services that depend on the abstract event-loop port.
class AsioEventLoop : public application::ports::IEventLoop {
public:
    AsioEventLoop();
    ~AsioEventLoop() override;

    AsioEventLoop(const AsioEventLoop&)            = delete;
    AsioEventLoop& operator=(const AsioEventLoop&) = delete;
    AsioEventLoop(AsioEventLoop&&)                 = delete;
    AsioEventLoop& operator=(AsioEventLoop&&)      = delete;

    // -----------------------------------------------------------------------
    // IEventLoop interface
    // -----------------------------------------------------------------------

    /// Run the io_context until `stop()` is called.
    /// Blocks the calling thread.  Returns after `stop()` causes the
    /// work guard to be released and all pending handlers have run.
    void run() override;

    /// Signal the io_context to stop.  `run()` / `run_for()` will return
    /// after the current handler (if any) completes.
    /// Safe to call from any thread.
    void stop() noexcept override;

    /// Post a callback to be executed on the io_context.
    /// Safe to call from any thread.
    void post(std::function<void()> fn) override;

    /// Returns true if the event loop is currently running.
    [[nodiscard]] bool is_running() const noexcept override;

    // -----------------------------------------------------------------------
    // Extended API (not part of IEventLoop)
    // -----------------------------------------------------------------------

    /// Run the io_context for at most `budget` milliseconds, then return.
    /// Useful for interleaving with a legacy event loop: the legacy loop
    /// calls this periodically to let Asio handlers make progress.
    ///
    /// Implementation note: we cannot rely on `io_context::run_for()` alone
    /// because it returns immediately when there is no pending work (the
    /// work guard prevents that, but we still want bounded execution).
    /// We therefore temporarily drop the work guard, call `run_for()`, and
    /// then re-install the guard.
    void run_for(std::chrono::milliseconds budget);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Return a reference to the underlying `asio::io_context`.
    /// Use this to construct sockets, timers, acceptors, etc.
    [[nodiscard]] boost::asio::io_context& io_context() noexcept;

private:
    using executor_type = boost::asio::io_context::executor_type;
    using work_guard_t  =
        boost::asio::executor_work_guard<executor_type>;

    boost::asio::io_context        ctx_;
    std::optional<work_guard_t>    work_guard_;
    std::atomic<bool>              running_{false};
};

}  // namespace pvpgn::app::bnetd
