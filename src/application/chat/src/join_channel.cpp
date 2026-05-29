// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/join_channel.hpp"

#include "application/ports/account_repository.hpp"
#include "application/ports/channel_repository.hpp"
#include "application/ports/session_registry.hpp"
#include "core/trace.hpp"
#include "domain/shared/events.hpp"

namespace pvpgn::application::chat {

core::Result<JoinChannelResult, JoinChannelError>
JoinChannel::execute(domain::AccountId account_id, const std::string& channel_name,
                     domain::ClientTag client_tag) const {
    PVPGN_SPAN("JoinChannel");

    // 0. Validate channel name
    if (channel_name.empty() || channel_name.find('\0') != std::string::npos ||
        channel_name.find('\x01') != std::string::npos) {
        return core::fail(JoinChannelError::InvalidChannelName);
    }

    // 1. Try to find existing channel by name
    auto found = channel_repo_.find_by_name(channel_name);

    domain::chat::Channel channel = [&]() {
        if (found) {
            return found.value();
        }
        // Create new public channel if not found
        domain::chat::Channel new_ch = domain::chat::Channel::create(
            domain::ChannelId{0},  // ID will be assigned by repo
            channel_name,
            domain::chat::ChannelPolicy{
                .flags = {},
                .max_members = 0,  // unlimited
                .client = domain::ClientTag{},
            });
        return new_ch;
    }();

    // 2. Attempt to add member to channel
    auto outcome = channel.admit(account_id, client_tag);

    // Map domain outcome to application error
    switch (outcome) {
        case domain::chat::Channel::JoinOutcome::Locked:
            return core::fail(JoinChannelError::Locked);
        case domain::chat::Channel::JoinOutcome::Banned:
            return core::fail(JoinChannelError::Banned);
        case domain::chat::Channel::JoinOutcome::WrongClientTag:
            return core::fail(JoinChannelError::WrongClientTag);
        case domain::chat::Channel::JoinOutcome::Full:
            return core::fail(JoinChannelError::Full);
        case domain::chat::Channel::JoinOutcome::Accepted:
            break;
    }

    // 3. Save updated channel to repository
    auto save_result = channel_repo_.save(channel);
    if (!save_result) {
        return core::fail(JoinChannelError::NotFound);
    }

    // 4. Look up account name for client reply
    auto account_result = account_repo_.find_by_id(account_id);
    if (!account_result) {
        return core::fail(JoinChannelError::AccountNotFound);
    }

    // 5. Retrieve the saved channel from repository to get the assigned ID
    auto saved_channel_result = channel_repo_.find_by_name(channel_name);
    if (!saved_channel_result) {
        return core::fail(JoinChannelError::NotFound);
    }
    auto saved_channel = saved_channel_result.value();

    // 6. Drain domain events and collect member session IDs via session registry
    auto events = saved_channel.drain_events();
    std::vector<domain::SessionId> members_to_notify;

    auto member_ids = saved_channel.member_ids();
    for (const auto& member_id : member_ids) {
        if (member_id.value() != account_id.value()) {
            // Look up the real SessionId for this member from the session registry.
            // Members without an active session are silently skipped.
            if (auto sid = session_registry_.session_for(member_id)) {
                members_to_notify.push_back(sid.value());
            }
        }
    }
    (void)events;  // Events will be processed by caller

    return JoinChannelResult{
        .channel = saved_channel,
        .members_to_notify = members_to_notify,
        .flags = saved_channel.policy().flags,
    };
}

}  // namespace pvpgn::application::chat
