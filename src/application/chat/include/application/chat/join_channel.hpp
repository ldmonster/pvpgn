// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file join_channel.hpp
/// JOIN_CHANNEL use-case — client joins an existing or new channel.
///
/// Orchestrates between repositories and the Channel aggregate.
/// Returns decision + side effects (domain events to be processed by caller).

#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"
#include "application/ports/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur during channel join.
enum class JoinChannelError : std::uint8_t {
    NotFound,
    Full,
    Banned,
    WrongClientTag,
    Locked,
    AccountNotFound,
    InvalidChannelName,
};

/// Result of a successful join.
struct JoinChannelResult {
    /// The updated channel snapshot.
    domain::chat::Channel channel;
    /// List of session IDs to notify about this join (other members).
    std::vector<domain::SessionId> members_to_notify;
    /// Channel flags for reply encoding.
    domain::chat::ChannelFlags flags;
};

class JoinChannel {
public:
    explicit JoinChannel(application::ports::IChannelRepository& channel_repo,
                         application::ports::IAccountRepository& account_repo,
                         application::ports::ISessionRegistry& session_registry)
        : channel_repo_(channel_repo), account_repo_(account_repo),
          session_registry_(session_registry) {}

    /// Execute: attempt to join or create channel.
    /// Caller is responsible for encoding the domain events.
    [[nodiscard]] core::Result<JoinChannelResult, JoinChannelError>
    execute(domain::AccountId account_id, const std::string& channel_name,
            domain::ClientTag client_tag) const;

private:
    application::ports::IChannelRepository&  channel_repo_;
    application::ports::IAccountRepository&  account_repo_;
    application::ports::ISessionRegistry&    session_registry_;
};

}  // namespace pvpgn::application::chat
