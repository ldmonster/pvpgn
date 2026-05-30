// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_repository.hpp
/// Application-layer port for the chat-channel store.

#include <cstddef>
#include <functional>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

class IChannelRepository {
public:
    virtual ~IChannelRepository() = default;

    IChannelRepository(const IChannelRepository&)            = delete;
    IChannelRepository& operator=(const IChannelRepository&) = delete;
    IChannelRepository(IChannelRepository&&)                 = delete;
    IChannelRepository& operator=(IChannelRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<domain::chat::Channel>
    find_by_id(domain::ChannelId id) const = 0;

    [[nodiscard]] virtual core::Result<domain::chat::Channel>
    find_by_name(const std::string& name) const = 0;

    virtual core::Status<> save(const domain::chat::Channel& channel) = 0;

    virtual core::Status<> remove(domain::ChannelId id) = 0;

    virtual void forEach(
        std::function<bool(const domain::chat::Channel&)> predicate) const = 0;

    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

protected:
    IChannelRepository() = default;
};

} // namespace pvpgn::application::ports
