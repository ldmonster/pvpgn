// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file leave_channel.hpp
/// LEAVE_CHANNEL use-case — client leaves a channel.
///
/// Removes account from channel. If channel becomes empty and is not
/// permanent, the channel is deleted from the repository.
/// Returns the list of remaining members to notify.

#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/chat/ports.hpp"
#include "domain/identity/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur during channel leave.
enum class LeaveChannelError : std::uint8_t {
    ChannelNotFound,
    NotInChannel,
};

/// Result of a successful leave.
struct LeaveChannelResult {
    /// List of session IDs of remaining members to notify.
    std::vector<domain::SessionId> members_to_notify;
    /// Whether the channel was deleted (empty + non-permanent).
    bool channel_deleted = false;
};

class LeaveChannel {
public:
    explicit LeaveChannel(domain::chat::IChannelRepository& channel_repo,
                          domain::identity::ISessionRegistry& session_registry)
        : channel_repo_(channel_repo), session_registry_(session_registry) {}

    /// Execute: remove account from channel.
    /// If channel becomes empty and is non-permanent, it is deleted.
    [[nodiscard]] core::Result<LeaveChannelResult, LeaveChannelError>
    execute(domain::ChannelId channel_id, domain::AccountId account_id) const;

private:
    domain::chat::IChannelRepository& channel_repo_;
    domain::identity::ISessionRegistry&   session_registry_;
};

}  // namespace pvpgn::application::chat
