// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file post_message.hpp
/// POST_MESSAGE use-case — send a message to a channel.
///
/// Validates account membership and account flags (muted/banned),
/// then broadcasts the message via the Channel aggregate.
/// Returns the list of session IDs to notify + the domain event.

#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "application/ports/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur during message posting.
enum class PostMessageError : std::uint8_t {
    ChannelNotFound,
    NotInChannel,
    AccountMuted,
    AccountBanned,
};

/// Result of a successful message post.
struct PostMessageResult {
    /// The chat message domain event to broadcast.
    domain::events::ChannelMessageSent event;
    /// List of session IDs that should receive this message.
    std::vector<domain::SessionId> recipients;
};

class PostMessage {
public:
    explicit PostMessage(application::ports::IChannelRepository& channel_repo,
                         application::ports::ISessionRegistry& session_registry)
        : channel_repo_(channel_repo), session_registry_(session_registry) {}

    /// Execute: validate membership and post message to channel.
    [[nodiscard]] core::Result<PostMessageResult, PostMessageError>
    execute(domain::ChannelId channel_id, domain::AccountId account_id,
            const domain::ChatMessage& message) const;

private:
    application::ports::IChannelRepository& channel_repo_;
    application::ports::ISessionRegistry&   session_registry_;
};

}  // namespace pvpgn::application::chat
