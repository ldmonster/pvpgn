// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/post_message.hpp"

#include "application/ports/channel_repository.hpp"
#include "application/ports/session_registry.hpp"

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
        (void)ev;  // Events will be processed by caller
    }

    // 6. Build notification list (all channel members except sender)
    // Look up real SessionIds via the session registry; skip members without
    // an active session.
    std::vector<domain::SessionId> recipients;
    auto member_ids = channel.member_ids();
    for (const auto& member_id : member_ids) {
        if (member_id.value() != account_id.value()) {
            if (auto sid = session_registry_.session_for(member_id)) {
                recipients.push_back(sid.value());
            }
        }
    }

    return PostMessageResult{
        .event = msg_event,
        .recipients = recipients,
    };
}

}  // namespace pvpgn::application::chat
