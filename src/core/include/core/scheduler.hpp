// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file scheduler.hpp
/// Abstract scheduler interface — replaces the legacy `t_timer` linked
/// list. The production implementation lives in `infrastructure/async/`
/// (Asio-backed, Phase 2). For Phase 1, a `ManualScheduler` is provided
/// for tests so domain code can be exercised deterministically without
/// pulling Asio in.

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "clock.hpp"

namespace pvpgn::core {

using Duration  = std::chrono::nanoseconds;
using TimerId   = std::uint64_t;

class IScheduler {
public:
    virtual ~IScheduler() = default;

    /// Schedule `fn` to run once after `delay` has elapsed. Returns a
    /// handle the caller can pass to `cancel()` to abort the timer if it
    /// has not yet fired.
    virtual TimerId schedule_after(Duration delay,
                                   std::function<void()> fn) = 0;

    /// Cancel a pending timer. No-op if the timer has already fired or
    /// been cancelled.
    virtual void cancel(TimerId) = 0;
};

/// Deterministic scheduler for tests. Timers fire when `advance()` moves
/// the clock past their deadline. Not thread-safe by design.
class ManualScheduler final : public IScheduler {
public:
    explicit ManualScheduler(std::shared_ptr<ManualClock> clock)
        : clock_(std::move(clock)) {}

    TimerId schedule_after(Duration delay,
                           std::function<void()> fn) override {
        const auto deadline = clock_->monotonic() + delay;
        const TimerId id = ++next_id_;
        timers_.emplace(deadline, Entry{id, std::move(fn)});
        return id;
    }

    void cancel(TimerId id) override {
        for (auto it = timers_.begin(); it != timers_.end(); ++it) {
            if (it->second.id == id) {
                timers_.erase(it);
                return;
            }
        }
    }

    /// Advance the underlying clock and fire any timers whose deadline
    /// has been reached, in order.
    template <class Rep, class Period>
    std::size_t advance(std::chrono::duration<Rep, Period> d) {
        clock_->advance(d);
        std::size_t fired = 0;
        while (!timers_.empty() &&
               timers_.begin()->first <= clock_->monotonic()) {
            auto node = timers_.extract(timers_.begin());
            auto fn = std::move(node.mapped().fn);
            if (fn) {
                fn();
                ++fired;
            }
        }
        return fired;
    }

    std::size_t pending() const noexcept { return timers_.size(); }

private:
    struct Entry {
        TimerId id;
        std::function<void()> fn;
    };

    std::shared_ptr<ManualClock> clock_;
    std::multimap<IClock::MonotonicTime, Entry> timers_;
    TimerId next_id_ = 0;
};

}  // namespace pvpgn::core
