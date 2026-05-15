// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/leave_channel.hpp"

#include "application/ports/channel_repository.hpp"

namespace pvpgn::application::chat {

core::Result<LeaveChannelResult, LeaveChannelError>
LeaveChannel::execute(domain::ChannelId channel_id, domain::AccountId account_id) const {
    // 1. Find the channel
    auto found = channel_repo_.find_by_id(channel_id);
    if (!found) {
        return core::fail(LeaveChannelError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 2. Validate account is a member
    if (!channel.contains(account_id)) {
        return core::fail(LeaveChannelError::NotInChannel);
    }

    // 3. Remove member from channel
    channel.leave(account_id);

    // 4. Check if channel is now empty and non-permanent
    bool should_delete = channel.member_count() == 0 &&
                         !channel.policy().flags.has(domain::chat::ChannelFlag::Permanent);

    if (should_delete) {
        // Delete the channel from repository
        auto remove_result = channel_repo_.remove(channel_id);
        if (!remove_result) {
            return core::fail(LeaveChannelError::ChannelNotFound);
        }
    } else {
        // Save updated channel
        auto save_result = channel_repo_.save(channel);
        if (!save_result) {
            return core::fail(LeaveChannelError::ChannelNotFound);
        }
    }

    // 5. Drain domain events and collect remaining member session IDs
    auto events = channel.drain_events();
    std::vector<domain::SessionId> members_to_notify;
    // In real implementation, would look up session IDs from connection registry

    return LeaveChannelResult{
        .members_to_notify = members_to_notify,
        .channel_deleted = should_delete,
    };
}

}  // namespace pvpgn::application::chat
