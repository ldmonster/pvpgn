// SPDX-License-Identifier: GPL-2.0-or-later

/// @file logout_user.cpp
/// Implementation of LogoutUser use-case.
///
/// R305: Channel cleanup on disconnect.
/// When `leave_channel_` is injected, the execute() method iterates
/// `IChannelRepository` to find any channel the account is currently in
/// and calls `LeaveChannel::execute()` to remove the membership.
/// This prevents ghost members in the channel roster after a disconnect.

#include "application/auth/logout_user.hpp"

#include "application/chat/leave_channel.hpp"

namespace pvpgn::application::auth {

LogoutUser::Result LogoutUser::execute(const LogoutRequest& req) {
    // 1. Verify the session exists
    if (!sessions_.account_for(req.session_id).has_value()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "Session not found"});
    }

    // 2. R305: Channel cleanup — remove the account from any channel it is in.
    //    Iterate all channels; for each one that contains this account, call
    //    LeaveChannel::execute().  We collect the channel IDs first to avoid
    //    mutating the repository while iterating.
    if (leave_channel_ != nullptr) {
        std::vector<domain::ChannelId> joined_channels;

        channels_.forEach([&](const domain::chat::Channel& ch) -> bool {
            if (ch.contains(req.account_id)) {
                joined_channels.push_back(ch.id());
            }
            return true;  // continue iteration
        });

        for (const auto& channel_id : joined_channels) {
            // Ignore errors — the channel may have been deleted concurrently.
            (void)leave_channel_->execute(channel_id, req.account_id);
        }
    }

    // 3. Detach the session from the registry.
    sessions_.detach(req.session_id);

    // 4. Note: Game cleanup would happen here via games_.forEach().
    //    Deferred to a full implementation that has access to the complete
    //    domain types.
    //
    // Future: games_.forEach(...game.leave...)

    return core::ok();
}

}  // namespace pvpgn::application::auth
