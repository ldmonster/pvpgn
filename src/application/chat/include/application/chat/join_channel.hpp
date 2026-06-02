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
#include "domain/chat/ports.hpp"
#include "domain/identity/ports.hpp"

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
    explicit JoinChannel(domain::chat::IChannelRepository& channel_repo,
                         domain::identity::IAccountRepository& account_repo,
                         domain::identity::ISessionRegistry& session_registry)
        : channel_repo_(channel_repo), account_repo_(account_repo),
          session_registry_(session_registry) {}

    /// Execute: attempt to join or create channel.
    /// Caller is responsible for encoding the domain events.
    [[nodiscard]] core::Result<JoinChannelResult, JoinChannelError>
    execute(domain::AccountId account_id, const std::string& channel_name,
            domain::ClientTag client_tag) const;

private:
    domain::chat::IChannelRepository&  channel_repo_;
    domain::identity::IAccountRepository&  account_repo_;
    domain::identity::ISessionRegistry&    session_registry_;
};

}  // namespace pvpgn::application::chat
