// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_config_subscriber.hpp
/// In-memory fake for IConfigSubscriber.
/// Captures reload notifications and allows test-driven triggering.
/// Suitable for tests and development/CI composition roots.

#include <functional>
#include <mutex>
#include <vector>

#include "domain/shared/ports/config_subscriber.hpp"

namespace pvpgn::infra::inmemory {

/// In-memory fake that records every config-reload notification.
/// Tests can register additional callbacks via `on_reload()` and
/// trigger a synthetic reload via `notify(config)`.
class InMemoryConfigSubscriber final
    : public domain::shared::ports::IConfigSubscriber {
public:
    /// Return the number of times on_config_reloaded has been called.
    [[nodiscard]] std::size_t reload_count() const {
        std::unique_lock lock(mutex_);
        return reload_count_;
    }

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
