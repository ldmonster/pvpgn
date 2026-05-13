// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_bus.hpp
/// Synchronous in-process event bus port.
///
/// The bus is intentionally minimal: handlers are invoked in
/// registration order from the same fiber that publishes. Threaded /
/// fiber-fan-out implementations belong in `infra/` and are wired by
/// composition root.

#include <cstdint>
#include <functional>

#include "domain/shared/events.hpp"

namespace pvpgn::application::ports {

using SubscriptionId = std::uint64_t;

using EventHandler = std::function<void(const domain::events::DomainEvent&)>;

class IEventBus {
public:
    virtual ~IEventBus() = default;

    /// Publish the event to every current subscriber in registration
    /// order. Exceptions thrown by handlers are caught by the
    /// implementation (logged + isolated) — they MUST NOT abort the
    /// publish loop.
    virtual void publish(const domain::events::DomainEvent& e) = 0;

    /// Register a handler. Returns a token usable with `unsubscribe()`.
    virtual SubscriptionId subscribe(EventHandler handler) = 0;

    virtual void unsubscribe(SubscriptionId id) = 0;
};

}  // namespace pvpgn::application::ports
