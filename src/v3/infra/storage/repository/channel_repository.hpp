// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_repository.hpp
/// Thread-safe in-memory store for channels. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "application/ports/channel_repository.hpp"

namespace pvpgn::infra::storage {

class InMemoryChannelRepository final
    : public application::ports::IChannelRepository {
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
        auto it = by_name_.find(name);
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
        by_name_[channel.name()] = channel.id().value();
        by_id_[channel.id().value()] = std::move(copy);
        return core::ok();
    }

    core::Status<> remove(domain::ChannelId id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "channel: id not found"});
        }
        by_name_.erase(it->second->name());
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
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<domain::chat::Channel>> by_id_;
    std::unordered_map<std::string, std::uint32_t> by_name_;
};

}  // namespace pvpgn::infra::storage
