// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/send_emote.hpp"

#include "domain/chat/ports.hpp"
#include "domain/connection/ports.hpp"
#include "domain/chat/channel.hpp"

namespace pvpgn::application::chat {

core::Result<SendEmoteResult, SendEmoteError>
SendEmote::execute(const SendEmoteRequest& req) const {
    // 1. Find the channel
    auto found = channels_->find_by_id(req.channel_id);
    if (!found) {
        return core::fail(SendEmoteError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 2. Validate sender is a member
    if (!channel.contains(req.sender_id)) {
        return core::fail(SendEmoteError::NotInChannel);
    }

    // 3. Post message to channel (same as regular message)
    bool accepted = channel.post(req.sender_id, domain::ChatMessage{req.emote_text});
    if (!accepted) {
        return core::fail(SendEmoteError::NotInChannel);
    }

    // 4. Save updated channel
    auto save_result = channels_->save(channel);
    if (!save_result) {
        return core::fail(SendEmoteError::ChannelNotFound);
    }

    // 5. Build notification list (all channel members including sender)
    std::vector<domain::SessionId> recipients;
    // In real implementation, would look up session IDs from connection registry
    // For now, return empty vector (caller will handle routing)

    // 6. Drain events and extract message event
    auto events = channel.drain_events();
    domain::events::ChannelMessageSent emote_event{
        req.channel_id, req.sender_id, domain::ChatMessage{req.emote_text}};

    return SendEmoteResult{
        .recipients = recipients,
        .event = emote_event,
    };
}

}  // namespace pvpgn::application::chat
