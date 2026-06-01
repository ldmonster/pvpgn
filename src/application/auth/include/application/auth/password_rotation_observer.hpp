// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file password_rotation_observer.hpp
/// Audit-trail observer for `AccountPasswordRotationRequired` /
/// `AccountPasswordRotationCleared` domain events (Batch 25c).
///
/// Wired by composition root: subscribe to the application event
/// bus, log every rotation flip as a structured record so operators
/// can answer "when was must_change_password set/cleared for this
/// account?" without combing through `eventlog`.

#include "domain/shared/event_bus.hpp"
#include "core/logging.hpp"
#include "domain/shared/events.hpp"

namespace pvpgn::application::auth {

class PasswordRotationObserver {
public:
    /// Caller retains ownership of both `logger` and `bus`.
    /// Lifetime: the observer must outlive any in-flight `publish()`.
    PasswordRotationObserver(core::ILogger& logger,
                             ports::IEventBus& bus);

    PasswordRotationObserver(const PasswordRotationObserver&) = delete;
    PasswordRotationObserver& operator=(
        const PasswordRotationObserver&) = delete;

    ~PasswordRotationObserver();

    /// Stable channel name used in every emitted record.
    static constexpr const char* kChannel =
        "auth.password_rotation";

private:
    void on_event(const domain::events::DomainEvent& e) noexcept;

    core::ILogger&       logger_;
    ports::IEventBus&    bus_;
    ports::SubscriptionId token_{0};
};

}  // namespace pvpgn::application::auth
