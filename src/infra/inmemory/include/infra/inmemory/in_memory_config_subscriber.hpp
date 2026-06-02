// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_config_subscriber.hpp
/// In-memory fake for IConfigSubscriber.
/// Captures reload notifications, lets tests register callbacks, and allows
/// test-driven triggering of synthetic reloads.
/// Suitable for tests and development/CI composition roots.

#include <cstddef>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "domain/shared/ports/config_subscriber.hpp"
#include "infra/config/server_config.hpp"

namespace pvpgn::infra::inmemory {

/// In-memory fake that records every config-reload notification and fans each
/// one out to any callbacks registered via `on_reload()`.
class InMemoryConfigSubscriber final
    : public domain::shared::ports::IConfigSubscriber {
public:
    using Callback = std::function<void(const infra::config::ServerConfig&)>;

    /// Return the number of reloads observed so far.
    [[nodiscard]] std::size_t reload_count() const {
        std::unique_lock lock(mutex_);
        return reload_count_;
    }

    /// Register a callback invoked on every subsequent `notify()`.
    void on_reload(Callback cb) {
        std::unique_lock lock(mutex_);
        callbacks_.push_back(std::move(cb));
    }

    /// Trigger a synthetic reload: bump the count and fan out to callbacks.
    void notify(const infra::config::ServerConfig& cfg) {
        std::vector<Callback> snapshot;
        {
            std::unique_lock lock(mutex_);
            ++reload_count_;
            snapshot = callbacks_;
        }
        for (auto& cb : snapshot) {
            cb(cfg);
        }
    }

    /// Convenience overload that forwards to `notify()`.
    void on_config_reloaded(const infra::config::ServerConfig& cfg) {
        notify(cfg);
    }

    /// IConfigSubscriber: count an argument-less reload notification.
    void on_config_reloaded() override {
        std::unique_lock lock(mutex_);
        ++reload_count_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<Callback> callbacks_;
    std::size_t reload_count_{0};
};

}  // namespace pvpgn::infra::inmemory
