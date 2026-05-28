// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_loop.hpp
/// Abstract event loop port.
///
/// Implementations (e.g. Asio-backed IoRuntime) live in `infra/`.
/// Null/fake implementations for tests live in `infra/inmemory/`.

#include <functional>

namespace pvpgn::application::ports {

class IEventLoop {
public:
    virtual ~IEventLoop() = default;

    /// Start the event loop (blocks until stop() is called).
    virtual void run() = 0;

    /// Signal the event loop to stop.
    virtual void stop() noexcept = 0;

    /// Post a callable to be executed on the event loop thread.
    virtual void post(std::function<void()> task) = 0;

    /// Returns true if currently running.
    virtual bool is_running() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
