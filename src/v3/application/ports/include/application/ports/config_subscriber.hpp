// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file config_subscriber.hpp
/// Port for receiving configuration reload notifications.

#include "infra/config/server_config.hpp"

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
