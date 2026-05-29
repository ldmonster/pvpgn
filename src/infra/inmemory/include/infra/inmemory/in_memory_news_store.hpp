// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_news_store.hpp
/// In-memory fake for INewsStore.
/// Suitable for tests and development/CI composition roots.

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "application/ports/news_store.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryNewsStore final
    : public application::ports::INewsStore {
public:
    core::Result<std::vector<application::ports::NewsItem>>
    get_news(std::size_t max_items) override {
        std::shared_lock lock(mutex_);
        // Items are stored oldest-first; return newest-first up to max_items.
        std::vector<application::ports::NewsItem> result;
        const std::size_t total = items_.size();
        const std::size_t count = std::min(max_items, total);
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            result.push_back(items_[total - 1 - i]);
        }
        return result;
    }

    core::Status<>
    add_news(application::ports::NewsItem item) override {
        std::unique_lock lock(mutex_);
        items_.push_back(std::move(item));
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    std::vector<application::ports::NewsItem> items_;
};

}  // namespace pvpgn::infra::inmemory
