// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_channel_store.hpp
/// In-memory fake for IChannelStore.
/// Suitable for tests and development/CI composition roots.

#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "domain/chat/ports.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryChannelStore final
    : public application::ports::IChannelStore {
public:
    [[nodiscard]] core::Status<std::vector<application::ports::ChannelDefinition>>
    load_all() const override {
        std::shared_lock lock(mutex_);
        std::vector<application::ports::ChannelDefinition> result;
        result.reserve(defs_.size());
        for (const auto& [name, def] : defs_) {
            result.push_back(def);
        }
        return result;
    }

    core::Status<void>
    save(application::ports::ChannelDefinition def) override {
        std::unique_lock lock(mutex_);
        defs_[def.name] = std::move(def);
        return core::ok();
    }

    core::Status<void>
    remove(std::string_view name) override {
        std::unique_lock lock(mutex_);
        auto it = defs_.find(std::string{name});
        if (it == defs_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "channel store: name not found"});
        }
        defs_.erase(it);
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string,
                       application::ports::ChannelDefinition> defs_;
};

}  // namespace pvpgn::infra::inmemory
