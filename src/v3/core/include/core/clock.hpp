// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clock.hpp
/// Abstract clock interface for the v3 tree. **All** code that needs
/// wall-clock or monotonic time outside `core/` must inject an `IClock`.
/// This eliminates the legacy `extern time_t now;` global and enables
/// deterministic time in tests.

#include <chrono>
#include <memory>

namespace pvpgn::core {

/// Wall-clock and monotonic time aliases at namespace scope so domain
/// code can use `core::SystemTime` directly without going through the
/// interface.
using SystemTime    = std::chrono::system_clock::time_point;
using MonotonicTime = std::chrono::steady_clock::time_point;

class IClock {
public:
    using SystemTime    = pvpgn::core::SystemTime;
    using MonotonicTime = pvpgn::core::MonotonicTime;

    virtual ~IClock() = default;

    /// Wall-clock time. Use for protocols and persistence timestamps.
    virtual SystemTime    now()        const noexcept = 0;

    /// Monotonic time. Use for measuring intervals; never goes backwards.
    virtual MonotonicTime monotonic()  const noexcept = 0;
};

/// Production implementation backed by the C++ standard clocks.
class SystemClock final : public IClock {
public:
    SystemTime    now()       const noexcept override { return std::chrono::system_clock::now(); }
    MonotonicTime monotonic() const noexcept override { return std::chrono::steady_clock::now(); }
};

/// Deterministic implementation for tests. Advances only on explicit
/// `advance()` calls; both clocks tick in lock-step.
class ManualClock final : public IClock {
public:
    explicit ManualClock(SystemTime start = {}) : sys_(start) {}

    SystemTime    now()       const noexcept override { return sys_; }
    MonotonicTime monotonic() const noexcept override {
        return MonotonicTime{std::chrono::nanoseconds{mono_ns_}};
    }

    void set(SystemTime t) noexcept { sys_ = t; }

    template <class Rep, class Period>
    void advance(std::chrono::duration<Rep, Period> d) noexcept {
        sys_ += std::chrono::duration_cast<SystemTime::duration>(d);
        mono_ns_ += std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
    }

private:
    SystemTime    sys_{};
    std::int64_t  mono_ns_ = 0;
};

inline std::shared_ptr<IClock> make_system_clock() {
    return std::make_shared<SystemClock>();
}

}  // namespace pvpgn::core
