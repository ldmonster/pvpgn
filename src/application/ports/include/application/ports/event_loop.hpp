// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_loop.hpp
/// Application-layer port for an asynchronous event loop.
///
/// Concrete implementations:
///   - `pvpgn::app::bnetd::AsioEventLoop` (boost::asio-based)
///   - test fakes in unit tests

#include <functional>

namespace pvpgn::application::ports {

class IEventLoop {
public:
    virtual ~IEventLoop() = default;

    /// Run the event loop until `stop()` is called.  Blocks the calling thread.
    virtual void run() = 0;

    /// Request the loop to stop.  Safe to call from any thread.
    virtual void stop() noexcept = 0;

    /// Post a callback to be executed on the event loop thread.
    /// Safe to call from any thread.
    virtual void post(std::function<void()> fn) = 0;

    /// Whether the loop is currently running.
    [[nodiscard]] virtual bool is_running() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
