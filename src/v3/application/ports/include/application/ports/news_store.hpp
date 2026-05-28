// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file news_store.hpp
/// Port: news / MOTD store for server-wide announcements.

#include <cstddef>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "core/result.hpp"

namespace pvpgn::application::ports {

/// A single news item shown to clients on login.
struct NewsItem {
    std::string      text;
    core::SystemTime published_at;
};

/// Port: news store interface for hexagonal architecture.
/// Implementations provide persistence backends (InMemory, SQLite, file, etc.).
class INewsStore {
public:
    virtual ~INewsStore() = default;

    /// Return up to `max_items` most-recent news items (newest first).
    virtual core::Result<std::vector<NewsItem>>
    get_news(std::size_t max_items) = 0;

    /// Append a new news item.
    virtual core::Status<>
    add_news(NewsItem item) = 0;
};

}  // namespace pvpgn::application::ports
