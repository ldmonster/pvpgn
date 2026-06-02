// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file set_channel_topic.hpp
/// SET_CHANNEL_TOPIC use-case — update a channel's topic/description.
///
/// Validates permissions (operator or admin), length, then sets via aggregate.
/// Broadcasts new topic to all members.

#include <cstddef>
#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/chat/ports.hpp"
#include "domain/connection/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur when setting topic.
enum class SetChannelTopicError : std::uint8_t {
    ChannelNotFound,
    InsufficientPermissions,
    TopicTooLong,  // > 255 chars
};

/// Request to set a channel's topic.
struct SetChannelTopicRequest {
    domain::AccountId setter_id;
    domain::ChannelId channel_id;
    std::string       new_topic;
};

class SetChannelTopic {
public:
    explicit SetChannelTopic(
        std::shared_ptr<domain::chat::IChannelRepository> channels,
        std::shared_ptr<domain::connection::IMessageRouter> router)
        : channels_(channels), router_(router) {}

    /// Execute: validate permissions and set channel topic.
    core::Result<void, SetChannelTopicError>
    execute(const SetChannelTopicRequest& req) const;

private:
    static constexpr std::size_t MAX_TOPIC_LENGTH = 255;

    std::shared_ptr<domain::chat::IChannelRepository> channels_;
    std::shared_ptr<domain::connection::IMessageRouter>     router_;
};

}  // namespace pvpgn::application::chat
