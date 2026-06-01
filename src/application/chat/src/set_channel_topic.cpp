// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/set_channel_topic.hpp"

#include "domain/chat/ports.hpp"
#include "domain/connection/ports.hpp"
#include "domain/chat/channel.hpp"

namespace pvpgn::application::chat {

core::Result<void, SetChannelTopicError>
SetChannelTopic::execute(const SetChannelTopicRequest& req) const {
    // 1. Validate topic length
    if (req.new_topic.length() > MAX_TOPIC_LENGTH) {
        return core::fail(SetChannelTopicError::TopicTooLong);
    }

    // 2. Find the channel
    auto found = channels_->find_by_id(req.channel_id);
    if (!found) {
        return core::fail(SetChannelTopicError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 3. Verify setter is a member (implicit permission check)
    if (!channel.contains(req.setter_id)) {
        return core::fail(SetChannelTopicError::InsufficientPermissions);
    }

    // 4. Call channel.set_topic() — domain enforces any permission rules
    channel.set_topic(req.setter_id, std::string{req.new_topic});

    // 5. Save updated channel
    auto save_result = channels_->save(channel);
    if (!save_result) {
        return core::fail(SetChannelTopicError::ChannelNotFound);
    }

    // 6. Drain and route events
    auto events = channel.drain_events();
    // Events would be routed here via router_
    // For now, just acknowledge success

    return {};
}

}  // namespace pvpgn::application::chat
