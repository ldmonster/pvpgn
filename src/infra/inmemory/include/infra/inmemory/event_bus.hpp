// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_bus.hpp
/// Synchronous, single-threaded in-memory implementation of
/// `ports::IEventBus`. Safe for unit tests and single-fiber composition.

#include <atomic>
#include <unordered_map>

#include "domain/shared/event_bus.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryEventBus final : public application::ports::IEventBus {
public:
    void publish(const domain::events::DomainEvent& e) override {
        // Snapshot — handlers may unsubscribe mid-publish.
        const auto snapshot = handlers_;
        for (const auto& [id, h] : snapshot) {
            (void)id;
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
        handlers_.emplace(id, std::move(handler));
        return id;
    }

    void unsubscribe(application::ports::SubscriptionId id) override {
        handlers_.erase(id);
    }

private:
    std::atomic<application::ports::SubscriptionId> next_id_{1};
    std::unordered_map<application::ports::SubscriptionId,
                       application::ports::EventHandler> handlers_;
};

}  // namespace pvpgn::infra::inmemory
