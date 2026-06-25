// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_repository.hpp
/// Thread-safe in-memory store for channels. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "domain/chat/ports.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryChannelRepository final
    : public domain::chat::IChannelRepository {
public:
    core::Result<domain::chat::Channel>
    find_by_id(domain::ChannelId id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "channel: id not found"});
        }
        return *it->second;
    }

    core::Result<domain::chat::Channel>
    find_by_name(const std::string& name) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        // Channel names match case-insensitively (original uses strcasecmp);
        // by_name_ is keyed on the lowercased name, while the stored Channel
        // retains its original-cased display name.
        auto it = by_name_.find(name_key(name));
        if (it == by_name_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "channel: name not found"});
        }
        auto a = by_id_.find(it->second);
        if (a == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::Internal, "channel: index corrupt"});
        }
        return *a->second;
    }

    core::Status<>
    save(const domain::chat::Channel& channel) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_unique<domain::chat::Channel>(channel);
        by_name_[name_key(channel.name())] = channel.id().value();
        by_id_[channel.id().value()] = std::move(copy);
        return core::ok();
    }

    core::Status<>
    remove(domain::ChannelId id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "channel: id not found"});
        }
        by_name_.erase(name_key(it->second->name()));
        by_id_.erase(it);
        return core::ok();
    }

    void forEach(std::function<bool(const domain::chat::Channel&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [id, channel] : by_id_) {
            if (!predicate(*channel)) break;
        }
    }

    std::size_t size() const noexcept override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return by_id_.size();
    }

private:
    /// Lowercased lookup key for case-insensitive channel-name matching.
    static std::string name_key(std::string_view name) {
        std::string key{name};
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        return key;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<domain::chat::Channel>> by_id_;
    std::unordered_map<std::string, std::uint32_t> by_name_;
};

}  // namespace pvpgn::infra::inmemory
