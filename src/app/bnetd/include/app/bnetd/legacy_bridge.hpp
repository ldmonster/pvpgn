// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_bridge.hpp
/// `LegacyBridge` — glue between the legacy fdwatch event loop and the v3
/// Asio `io_context`.
///
/// Design
/// ------
/// The legacy `server_run()` loop in `src/bnetd/server.cpp` is a blocking
/// `select()`-based loop.  During Phase 3 we want the v3 Asio handlers to
/// make progress even while the legacy loop is still running.  `LegacyBridge`
/// provides the interleaving mechanism:
///
///   1. At startup, `LegacyBridge::init(loop)` stores a reference to the
///      shared `AsioEventLoop`.
///   2. The legacy loop calls `LegacyBridge::instance().tick(budget_ms)`
///      once per iteration (via `server_tick_v3()`).
///   3. `tick()` calls `AsioEventLoop::run_for(budget)` which lets Asio
///      handlers run for up to `budget` milliseconds before returning.
///
/// Singleton
/// ---------
/// `LegacyBridge` is a singleton because the legacy C++ code cannot hold
/// object references across translation-unit boundaries without significant
/// refactoring.  The singleton is initialised once at startup and destroyed
/// at shutdown.
///
/// Compile guard
/// -------------
/// The entire implementation is guarded by `PVPGN_V3_BNETD_INTEGRATION`.
/// When that macro is not defined (legacy-only build), the header still
/// compiles but all methods are no-ops so that `server_v3_hook.cpp` can
/// include it unconditionally.

#include <chrono>

#ifdef PVPGN_V3_BNETD_INTEGRATION
#include "app/bnetd/asio_event_loop.hpp"
#endif

namespace pvpgn::app::bnetd {

#ifdef PVPGN_V3_BNETD_INTEGRATION

/// Singleton bridge between the legacy fdwatch loop and the v3 Asio loop.
class LegacyBridge {
public:
    LegacyBridge(const LegacyBridge&)            = delete;
    LegacyBridge& operator=(const LegacyBridge&) = delete;
    LegacyBridge(LegacyBridge&&)                 = delete;
    LegacyBridge& operator=(LegacyBridge&&)      = delete;

    // -----------------------------------------------------------------------
    // Singleton access
    // -----------------------------------------------------------------------

    /// Return the singleton instance.
    /// Precondition: `init()` must have been called before the first call
    /// to `instance()`.
    [[nodiscard]] static LegacyBridge& instance();

    /// Initialise the singleton with the shared event loop.
    /// Must be called exactly once at startup, before any call to
    /// `instance()` or `tick()`.
    static void init(AsioEventLoop& loop);

    /// Destroy the singleton.  Called at shutdown.
    static void shutdown();

    // -----------------------------------------------------------------------
    // Interleaving API
    // -----------------------------------------------------------------------

    /// Run the Asio io_context for at most `budget` milliseconds.
    /// Called from the legacy main loop once per iteration.
    void tick(std::chrono::milliseconds budget);

    /// Return a reference to the shared `asio::io_context`.
    [[nodiscard]] boost::asio::io_context& get_io_context() noexcept;

private:
    explicit LegacyBridge(AsioEventLoop& loop) noexcept;

    AsioEventLoop& loop_;

    friend class std::default_delete<LegacyBridge>;
    ~LegacyBridge() = default;
};

#else  // !PVPGN_V3_BNETD_INTEGRATION

/// Stub when v3 integration is disabled — all methods are no-ops.
class LegacyBridge {
public:
    LegacyBridge(const LegacyBridge&)            = delete;
    LegacyBridge& operator=(const LegacyBridge&) = delete;

    [[nodiscard]] static LegacyBridge& instance() {
        static LegacyBridge inst;
        return inst;
    }

    template <typename T>
    static void init(T& /*loop*/) {}
    static void shutdown() {}

    void tick(std::chrono::milliseconds /*budget*/) {}

private:
    LegacyBridge()  = default;
    ~LegacyBridge() = default;
};

#endif  // PVPGN_V3_BNETD_INTEGRATION

}  // namespace pvpgn::app::bnetd
