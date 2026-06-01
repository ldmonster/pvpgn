// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_bus.hpp
/// Application-layer port for publishing/subscribing to domain events.

#include <cstdint>
#include <functional>

#include "domain/shared/events.hpp"

namespace pvpgn::application::ports {

using SubscriptionId = std::uint64_t;
using EventHandler   = std::function<void(const domain::events::DomainEvent&)>;

class IEventBus {
public:
    virtual ~IEventBus() = default;

    IEventBus(const IEventBus&)            = delete;
    IEventBus& operator=(const IEventBus&) = delete;
    IEventBus(IEventBus&&)                 = delete;
    IEventBus& operator=(IEventBus&&)      = delete;

    virtual void publish(const domain::events::DomainEvent& e) = 0;

    [[nodiscard]] virtual SubscriptionId
    subscribe(EventHandler handler) = 0;

    virtual void unsubscribe(SubscriptionId id) = 0;

protected:
    IEventBus() = default;
};

} // namespace pvpgn::application::ports
