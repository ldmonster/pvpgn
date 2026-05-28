// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_helpfile_source.hpp
/// In-memory fake for IHelpfileSource.
/// Suitable for tests and development/CI composition roots.

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "application/ports/helpfile_source.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryHelpfileSource final
    : public application::ports::IHelpfileSource {
public:
    /// Register a help entry for a command (test helper).
    void set(std::string command_name, std::string text) {
        std::unique_lock lock(mutex_);
        entries_[std::move(command_name)] = std::move(text);
    }

    [[nodiscard]] std::optional<std::string>
    lookup(std::string_view command_name) const override {
        std::shared_lock lock(mutex_);
        auto it = entries_.find(std::string{command_name});
        if (it == entries_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::vector<std::string>
    all_commands() const override {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        result.reserve(entries_.size());
        for (const auto& [cmd, _] : entries_) {
            result.push_back(cmd);
        }
        return result;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> entries_;
};

}  // namespace pvpgn::infra::inmemory
