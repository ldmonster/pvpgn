// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file news_store.hpp
/// Port for the server "news of the day" log shown on login.

#include "core/error.hpp"
#include "core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::application::ports {

/// A single news entry, surfaced to clients on login.
struct NewsItem {
    std::uint64_t timestamp = 0;  ///< unix seconds the entry was posted
    std::string   text;
};

/// Persistence boundary for the news subsystem.
class INewsStore {
public:
    virtual ~INewsStore() = default;

    /// Returns the most-recent `max_items` entries, newest first.
    virtual core::Result<std::vector<NewsItem>>
        get_news(std::size_t max_items) = 0;

    /// Append a new news entry.
    virtual core::Status<> add_news(NewsItem item) = 0;
};

} // namespace pvpgn::application::ports
