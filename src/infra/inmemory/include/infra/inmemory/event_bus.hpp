// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_bus.hpp
/// Synchronous in-memory implementation of `ports::IEventBus`.
///
/// Thread-safe: bnetd shares a single `InMemoryEventBus` across all
/// connection-handler threads (the Asio worker pool), so `publish`,
/// `subscribe`, and `unsubscribe` may run concurrently. A `std::mutex`
/// guards every access to `handlers_`.
///
/// `publish` snapshots the handler list **under the lock**, then
/// releases the lock **before** invoking handlers. Handlers are never
/// called while the mutex is held, so a handler that re-enters
/// `publish`/`subscribe`/`unsubscribe` cannot self-deadlock, and a
/// handler unsubscribed mid-publish is still observed via the snapshot.

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "domain/shared/event_bus.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryEventBus final : public application::ports::IEventBus {
public:
    void publish(const domain::events::DomainEvent& e) override {
        // Snapshot the handlers UNDER the lock — handlers may
        // unsubscribe (or subscribe) mid-publish, and other threads may
        // mutate the map concurrently.
        std::vector<application::ports::EventHandler> snapshot;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            snapshot.reserve(handlers_.size());
            for (const auto& [id, h] : handlers_) {
                (void)id;
                snapshot.push_back(h);
            }
        }
        // Invoke handlers OUTSIDE the lock so a handler that re-enters
        // the bus cannot deadlock.
        for (const auto& h : snapshot) {
            try {
                h(e);
            } catch (...) {
                // Per port contract: isolation, not propagation.
            }
        }
    }

    application::ports::SubscriptionId
    subscribe(application::ports::EventHandler handler) override {
        const auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> guard(mutex_);
        handlers_.emplace(id, std::move(handler));
        return id;
    }

    void unsubscribe(application::ports::SubscriptionId id) override {
        std::lock_guard<std::mutex> guard(mutex_);
        handlers_.erase(id);
    }

private:
    mutable std::mutex mutex_;
    std::atomic<application::ports::SubscriptionId> next_id_{1};
    std::unordered_map<application::ports::SubscriptionId,
                       application::ports::EventHandler> handlers_;
};

}  // namespace pvpgn::infra::inmemory
