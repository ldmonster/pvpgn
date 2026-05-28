// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file null_event_loop.hpp
/// No-op event loop for unit tests.
///
/// `run()` returns immediately; `post()` calls the task synchronously.
/// Safe for single-threaded test fixtures that need an IEventLoop dependency.

#include <functional>

#include "application/ports/event_loop.hpp"

namespace pvpgn::infra::inmemory {

class NullEventLoop final : public application::ports::IEventLoop {
public:
    /// run() sets running_ true then immediately false — no blocking.
    void run() override {
        running_ = true;
        running_ = false;
    }

    void stop() noexcept override { running_ = false; }

    /// Executes the task synchronously on the calling thread.
    void post(std::function<void()> task) override {
        if (task) task();
    }

    bool is_running() const noexcept override { return running_; }

private:
    bool running_{false};
};

}  // namespace pvpgn::infra::inmemory
