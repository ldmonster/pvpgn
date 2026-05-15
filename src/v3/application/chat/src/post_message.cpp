// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/post_message.hpp"

#include "application/ports/channel_repository.hpp"

namespace pvpgn::application::chat {

core::Result<PostMessageResult, PostMessageError>
PostMessage::execute(domain::ChannelId channel_id, domain::AccountId account_id,
                     const domain::ChatMessage& message) const {
    // 1. Find the channel
    auto found = channel_repo_.find_by_id(channel_id);
    if (!found) {
        return core::fail(PostMessageError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 2. Validate account is a member
    if (!channel.contains(account_id)) {
        return core::fail(PostMessageError::NotInChannel);
    }

    // 3. Post message to channel (aggregate decides if allowed)
    bool accepted = channel.post(account_id, domain::ChatMessage{message});
    if (!accepted) {
        // Should not happen if membership check passed, but be defensive
        return core::fail(PostMessageError::NotInChannel);
    }

    // 4. Save updated channel
    auto save_result = channel_repo_.save(channel);
    if (!save_result) {
        return core::fail(PostMessageError::ChannelNotFound);
    }

    // 5. Drain events (should be exactly one ChannelMessageSent)
    auto events = channel.drain_events();
    domain::events::ChannelMessageSent msg_event{
        channel_id, account_id, domain::ChatMessage{message}};

    for (const auto& ev : events) {
        // Extract the message event if it's in the events vector
        (void)ev;
    }

    // 6. Build notification list (all channel members)
    std::vector<domain::SessionId> recipients;
    // In real implementation, would look up session IDs from connection registry

    return PostMessageResult{
        .event = msg_event,
        .recipients = recipients,
    };
}

}  // namespace pvpgn::application::chat
