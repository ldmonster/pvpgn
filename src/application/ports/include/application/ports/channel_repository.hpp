// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/chat/channel.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace pvpgn::domain::chat {
// Channel is now fully defined via include above
}
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace pvpgn::domain::chat {
// Channel is now fully defined via include above
}

namespace pvpgn::application::ports {

/// Port: Channel repository interface for hexagonal architecture.
class IChannelRepository {
public:
    virtual ~IChannelRepository() = default;

    /// Find channel by name.
    virtual core::Result<domain::chat::Channel>
        find_by_name(const std::string& name) const = 0;

    /// Find channel by ID.
    virtual core::Result<domain::chat::Channel>
        find_by_id(domain::ChannelId id) const = 0;

    /// Save or update a channel.
    virtual core::Status<>
        save(const domain::chat::Channel& channel) = 0;

    /// Remove channel by ID.
    virtual core::Status<>
        remove(domain::ChannelId id) = 0;

    /// Iterate over all channels.
    virtual void forEach(std::function<bool(const domain::chat::Channel&)> predicate) const = 0;

    /// Get the number of channels.
    virtual std::size_t size() const noexcept = 0;
};

} // namespace pvpgn::application::ports
