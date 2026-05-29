// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_icon_provider.hpp
/// In-memory fake for IIconProvider.
/// Suitable for tests and development/CI composition roots.

#include <cstddef>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "application/ports/icon_provider.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryIconProvider final
    : public application::ports::IIconProvider {
public:
    /// Register an icon tag for a client tag (test helper).
    void set_icon(std::string client_tag, std::string icon_tag) {
        std::unique_lock lock(mutex_);
        icons_[std::move(client_tag)] = std::move(icon_tag);
    }

    /// Set the raw icon table bytes (test helper).
    void set_raw_data(std::vector<std::byte> data) {
        std::unique_lock lock(mutex_);
        raw_data_ = std::move(data);
    }

    [[nodiscard]] std::optional<std::string>
    icon_for(std::string_view client_tag) const override {
        std::shared_lock lock(mutex_);
        auto it = icons_.find(std::string{client_tag});
        if (it == icons_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::span<const std::byte>
    raw_icon_data() const override {
        std::shared_lock lock(mutex_);
        return std::span<const std::byte>{raw_data_};
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> icons_;
    std::vector<std::byte> raw_data_;
};

}  // namespace pvpgn::infra::inmemory
