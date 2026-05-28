// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file config_subscriber.hpp
/// Port for receiving configuration reload notifications.
///
/// The `ServerConfig` aggregate currently lives in infra (large
/// kitchen-sink struct exposed by `infra/config/server_config.hpp`).
/// Until the configuration model is split into a
/// domain/application-side `ConfigSnapshot` value type and an
/// infra-side `ServerConfig` adapter, this port forward-declares
/// the infra type and passes it by reference. The forward
/// declaration keeps the port header compile-time independent of
/// infra (no `#include`), which is what the layering check
/// enforces. The "name-level" coupling that remains is documented
/// as future-work in [plans/r218-checklist.md] (config snapshot
/// extraction).

namespace pvpgn::infra::config {
struct ServerConfig;
}  // namespace pvpgn::infra::config

namespace pvpgn::application::ports {

/// Observer interface for configuration reload events.
/// Subscribers are notified when the server configuration is reloaded
/// and passes validation.
class IConfigSubscriber {
public:
    virtual ~IConfigSubscriber() = default;

    /// Called when the configuration has been successfully reloaded.
    /// This is called only after the new config has been validated.
    /// Implementations should update their runtime state based on the
    /// new configuration.
    ///
    /// @param new_config The newly loaded and validated configuration
    virtual void on_config_reloaded(const infra::config::ServerConfig& new_config) = 0;
};

}  // namespace pvpgn::application::ports
