// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file config_subscriber.hpp
/// Application-layer port for observing configuration hot-reload events.
///
/// The concrete `ServerConfig` type lives in `infra/config`; it is
/// forward-declared here so this port header has no infra dependency.

namespace pvpgn::infra::config {
struct ServerConfig;
} // namespace pvpgn::infra::config

namespace pvpgn::application::ports {

/// Observer notified when the on-disk configuration is reloaded and
/// successfully validated.
class IConfigSubscriber {
public:
    virtual ~IConfigSubscriber() = default;

    /// Called on the watcher thread when @p new_config has just been
    /// loaded and validated. Implementations must be cheap and
    /// non-blocking; long work should be dispatched to a worker.
    virtual void on_config_reloaded(
        const infra::config::ServerConfig& new_config) = 0;

protected:
    IConfigSubscriber() = default;
};

} // namespace pvpgn::application::ports
