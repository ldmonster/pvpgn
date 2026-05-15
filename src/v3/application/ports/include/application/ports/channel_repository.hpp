// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_repository.hpp
/// Persistence port for the `chat::Channel` aggregate.
/// (Forward-declared to avoid circular dependencies)

#include <cstddef>
#include <functional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::chat {
class Channel;
}

namespace pvpgn::application::ports {

class IChannelRepository {
public:
    virtual ~IChannelRepository() = default;

    /// Look up a channel by primary key.
    virtual core::Result<domain::chat::Channel>
    find_by_id(domain::ChannelId id) const = 0;

    /// Find a channel by name (case-insensitive).
    virtual core::Result<domain::chat::Channel>
    find_by_name(std::string_view name) const = 0;

    /// Upsert. Implementations are expected to be idempotent.
    virtual core::Status<>
    save(const domain::chat::Channel& channel) = 0;

    /// Remove a channel.
    virtual core::Status<>
    remove(domain::ChannelId id) = 0;

    /// Iterate over all channels, applying predicate. Early exit on
    /// predicate returning false.
    virtual void
    forEach(std::function<bool(domain::chat::Channel&)> predicate) = 0;

    virtual std::size_t size() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
