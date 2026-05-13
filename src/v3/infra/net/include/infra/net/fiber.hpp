// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fiber.hpp
/// Optional Boost.Fiber integration. Compiled into `infra_net` only when
/// `PVPGN_V3_WITH_FIBER=ON`. The default v3 build does not pull Fiber.
///
/// Rationale: per migration plan §15.2 / §6, the network layer wants
/// "synchronous-style" coroutines per session so the code reads linearly
/// while the runtime multiplexes thousands of sessions over a small
/// thread pool. We pick Boost.Fiber because it ships in 1.83 (already a
/// dep transitively via Boost.Context) and integrates cleanly with
/// Asio's `asio::use_future`/`use_awaitable` idiom via fiber-aware
/// futures.
///
/// What lives here
/// ---------------
///   * `spawn_on(IoRuntime&, F)` — schedules a fiber on a runtime
///     worker. The fiber may call blocking primitives without blocking
///     the OS thread.
///   * `yield()` / `sleep_for()` — thin re-exports so callers don't
///     have to include `<boost/fiber/...>` directly.
///
/// Everything is a no-op when the flag is OFF: the header is still
/// includable but the spawn API is gated behind the macro so misuse
/// fails fast at compile time.

#include "infra/net/io_runtime.hpp"

#if defined(PVPGN_V3_HAVE_FIBER)

#include <boost/fiber/all.hpp>

namespace pvpgn::infra::net::fiber {

/// Spawn @p fn as a fiber on @p rt's executor. The fiber runs until @p fn
/// returns; resources held by the lambda must outlive that call (use
/// `shared_ptr` capture where needed).
template <class F>
void spawn_on(IoRuntime& rt, F&& fn) {
    rt.post([fn = std::forward<F>(fn)]() mutable {
        boost::fibers::fiber{std::move(fn)}.detach();
    });
}

inline void yield() { boost::this_fiber::yield(); }

template <class Rep, class Period>
void sleep_for(const std::chrono::duration<Rep, Period>& d) {
    boost::this_fiber::sleep_for(d);
}

}  // namespace pvpgn::infra::net::fiber

#endif  // PVPGN_V3_HAVE_FIBER
