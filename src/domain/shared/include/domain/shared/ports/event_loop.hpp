// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_loop.hpp
/// Application-layer port for an event loop / executor.
///
/// Abstracts the runtime that drives asynchronous work so the application
/// layer can post callbacks without depending on a concrete I/O backend
/// (asio in production, a synchronous null loop in tests).

#include <functional>

namespace pvpgn::application::ports {

/// Drives asynchronous work and lets callers schedule callbacks onto it.
class IEventLoop {
public:
    virtual ~IEventLoop() = default;

    /// Run the loop until stop() is called. Blocks the calling thread.
    virtual void run() = 0;

    /// Request the loop to stop. Safe to call from any thread.
    virtual void stop() noexcept = 0;

    /// Schedule `fn` to run on the loop.
    virtual void post(std::function<void()> fn) = 0;

    /// Whether the loop is currently running.
    [[nodiscard]] virtual bool is_running() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
