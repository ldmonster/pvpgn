// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file config_watcher.hpp
/// File-based configuration watcher with hot-reload support.

#include <chrono>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "domain/shared/ports/config_subscriber.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "infra/config/server_config.hpp"

namespace pvpgn::infra::config {

/// Watches a configuration file and notifies subscribers on hot-reload.
/// Periodically checks for file changes and reloads if needed.
/// All configuration changes are validated before notifying subscribers.
class ConfigWatcher {
public:
    /// Create a config watcher for the given file.
    /// @param config_path Path to the configuration file (e.g., TOML)
    explicit ConfigWatcher(std::string_view config_path);

    ~ConfigWatcher();

    // Non-copyable
    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;

    // Movable
    ConfigWatcher(ConfigWatcher&& other) noexcept;
    ConfigWatcher& operator=(ConfigWatcher&& other) noexcept;

    /// Register a subscriber to be notified on config reload.
    /// Subscribers are stored as weak_ptr to avoid circular references.
    /// @param subscriber Observer to add to the subscriber list
    void subscribe(std::weak_ptr<domain::shared::ports::IConfigSubscriber> subscriber);

    /// Unsubscribe all expired (dangling) weak_ptr subscribers.
    /// Called automatically during notification; can be called manually to clean up.
    void cleanup_expired_subscribers();

    /// Force a reload of the configuration now.
    /// Reads and parses the config file, validates it, and notifies subscribers
    /// if the new config is valid. If validation fails, the previous config is
    /// retained and an error is returned.
    /// @return Error if reload/validation fails; Ok otherwise
    core::Result<void, core::Error> reload();

    /// Start watching for file changes.
    /// Spawns a timer task that calls reload() every `interval` duration.
    /// If already watching, this is a no-op.
    /// @param interval Polling interval for checking file modifications
    void start_watch(std::chrono::seconds interval = std::chrono::seconds(5));

    /// Stop watching for file changes.
    /// Cancels the timer task if it's running.
    void stop_watch();

    /// Get the current (loaded) configuration.
    /// Thread-safe; acquires a shared lock. Returns a snapshot pointer
    /// that callers may hold past a subsequent `reload()` — the watcher
    /// will swap its internal pointer rather than mutate the snapshot.
    /// May return nullptr if no successful load has happened yet.
    [[nodiscard]] std::shared_ptr<const ServerConfig> current() const;

private:
    std::string config_path_;
    std::shared_ptr<const ServerConfig> current_config_;
    std::vector<std::weak_ptr<domain::shared::ports::IConfigSubscriber>> subscribers_;
    mutable std::shared_mutex mu_;

    // File watching state
    bool watching_{false};
    std::filesystem::file_time_type last_write_time_;

    /// Notify all live subscribers of a config change.
    /// Dead (expired) weak_ptr entries are skipped.
    void notify_subscribers(const ServerConfig& cfg);
};

}  // namespace pvpgn::infra::config
