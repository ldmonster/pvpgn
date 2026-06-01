// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/config/config_watcher.hpp"

#include <algorithm>
#include <filesystem>
#include <shared_mutex>

namespace pvpgn::infra::config {

ConfigWatcher::ConfigWatcher(std::string_view config_path)
    : config_path_(config_path) {
    // Try to load the initial config
    (void)reload();  // Ignore load errors at construction; can retry later
}

ConfigWatcher::~ConfigWatcher() {
    stop_watch();
}

ConfigWatcher::ConfigWatcher(ConfigWatcher&& other) noexcept
    : config_path_(std::move(other.config_path_)),
      current_config_(std::move(other.current_config_)),
      subscribers_(std::move(other.subscribers_)),
      watching_(other.watching_) {
    other.watching_ = false;
}

ConfigWatcher& ConfigWatcher::operator=(ConfigWatcher&& other) noexcept {
    if (this != &other) {
        stop_watch();
        config_path_ = std::move(other.config_path_);
        current_config_ = std::move(other.current_config_);
        subscribers_ = std::move(other.subscribers_);
        watching_ = other.watching_;
        other.watching_ = false;
    }
    return *this;
}

void ConfigWatcher::subscribe(
    std::weak_ptr<domain::shared::ports::IConfigSubscriber> subscriber) {
    std::unique_lock lock(mu_);
    subscribers_.push_back(subscriber);
}

void ConfigWatcher::cleanup_expired_subscribers() {
    std::unique_lock lock(mu_);
    auto it = std::remove_if(
        subscribers_.begin(), subscribers_.end(),
        [](const auto& wp) { return wp.expired(); });
    subscribers_.erase(it, subscribers_.end());
}

core::Result<void, core::Error> ConfigWatcher::reload() {
    // Load and parse the config file
    auto result = load_server_config(std::filesystem::path(config_path_));
    if (!result) {
        return core::fail(result.error());
    }

    // ServerConfig contains non-copyable `Secret<T>` members; move the
    // freshly-parsed value into an immutable, shareable snapshot.
    auto new_snapshot =
        std::make_shared<const ServerConfig>(std::move(result).value());

    // Update the published pointer atomically under the writer lock.
    {
        std::unique_lock lock(mu_);
        current_config_ = new_snapshot;
    }

    notify_subscribers(*new_snapshot);
    return core::ok();
}

void ConfigWatcher::start_watch(std::chrono::seconds interval) {
    // TODO: Implement file watching using boost::asio steady_timer
    // or std::thread with periodic polling.
    // For now, this is a placeholder.
    watching_ = true;
}

void ConfigWatcher::stop_watch() {
    watching_ = false;
    // TODO: Cancel any running timer/thread
}

std::shared_ptr<const ServerConfig> ConfigWatcher::current() const {
    std::shared_lock lock(mu_);
    return current_config_;
}

void ConfigWatcher::notify_subscribers(const ServerConfig& cfg) {
    std::shared_lock lock(mu_);
    for (auto& wp : subscribers_) {
        if (auto sp = wp.lock()) {
            sp->on_config_reloaded();
        }
    }
}

}  // namespace pvpgn::infra::config
