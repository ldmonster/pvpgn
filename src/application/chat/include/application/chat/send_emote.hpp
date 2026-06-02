// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_emote.hpp
/// SEND_EMOTE use-case — broadcast an emote/action to a channel.
///
/// Similar to PostMessage but emits EID_EMOTE instead of EID_TALK.

#include <memory>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "domain/chat/ports.hpp"
#include "domain/connection/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur during emote sending.
enum class SendEmoteError : std::uint8_t {
    ChannelNotFound,
    NotInChannel,
    AccountMuted,
    AccountBanned,
};

/// Request to send an emote to a channel.
struct SendEmoteRequest {
    domain::AccountId    sender_id;
    domain::ChannelId    channel_id;
    domain::ChatMessage  emote_text;
    domain::SessionId    sender_session;
};

/// Result of a successful emote send.
struct SendEmoteResult {
    /// List of session IDs that should receive the emote.
    std::vector<domain::SessionId> recipients;
    /// The domain event produced (for testing/logging).
    domain::events::ChannelMessageSent event;
};

class SendEmote {
public:
    explicit SendEmote(
        std::shared_ptr<domain::chat::IChannelRepository> channels,
        std::shared_ptr<domain::connection::IMessageRouter> router)
        : channels_(channels), router_(router) {}

    /// Execute: validate membership and send emote to channel.
    core::Result<SendEmoteResult, SendEmoteError>
    execute(const SendEmoteRequest& req) const;

private:
    std::shared_ptr<domain::chat::IChannelRepository> channels_;
    std::shared_ptr<domain::connection::IMessageRouter>     router_;
};

}  // namespace pvpgn::application::chat
