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
#include "domain/chat/topic_store.hpp"
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
        std::shared_ptr<domain::connection::IMessageRouter> router,
        std::shared_ptr<domain::chat::ITopicStore> topic_store = nullptr)
        : channels_(channels), router_(router),
          topic_store_(std::move(topic_store)) {}

    /// Execute: validate permissions and set channel topic.
    core::Result<void, SetChannelTopicError>
    execute(const SetChannelTopicRequest& req) const;

private:
    static constexpr std::size_t MAX_TOPIC_LENGTH = 255;

    std::shared_ptr<domain::chat::IChannelRepository> channels_;
    std::shared_ptr<domain::connection::IMessageRouter>     router_;
    // Channel-NAME-keyed persistent topic store. When present, the topic is
    // also written here so it outlives the Channel object (parity with the
    // original's class_topiclist). Null in stub/test mode.
    std::shared_ptr<domain::chat::ITopicStore>         topic_store_;
};

}  // namespace pvpgn::application::chat
