// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file config_subscriber.hpp
/// Port interface for configuration change notifications.

#include <memory>

namespace pvpgn::domain::shared::ports {

/// Observer interface for configuration changes.
/// Implementations subscribe to ConfigWatcher to receive hot-reload notifications.
class IConfigSubscriber {
public:
    virtual ~IConfigSubscriber() = default;

    /// Called when the configuration has been reloaded.
    /// Implementations should update their internal state based on the new configuration.
    virtual void on_config_reloaded() = 0;
};

}  // namespace pvpgn::domain::shared::ports
