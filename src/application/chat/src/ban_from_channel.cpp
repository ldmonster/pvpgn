// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/ban_from_channel.hpp"

#include "domain/chat/ports.hpp"
#include "domain/chat/channel.hpp"
#include "domain/moderation/ports.hpp"

namespace pvpgn::application::chat {

core::Result<void, BanFromChannelError>
BanFromChannel::execute(const BanFromChannelRequest& req) const {
    // 1. Validate not self-ban
    if (req.banner_id == req.target_id) {
        return core::fail(BanFromChannelError::CannotBanSelf);
    }

    // 2. Authorize the banner. The original `_handle_ban_command` requires the
    //    actor to be a channel admin / operator / tempOP; gate on the same
    //    "operator" command group used by OpFromChannel / KickFromChannel.
    if (!permissions_->has_command_group(req.banner_id, "operator")) {
        return core::fail(BanFromChannelError::NotAuthorized);
    }

    // 3. Find the channel
    auto found = channels_->find_by_id(req.channel_id);
    if (!found) {
        return core::fail(BanFromChannelError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 4. Verify banner is a member
    if (!channel.contains(req.banner_id)) {
        return core::fail(BanFromChannelError::InsufficientPermissions);
    }

    // 5. Operator/admin immunity: the original refuses to ban administrators
    //    or operators. Reject if the target holds either group.
    if (permissions_->has_command_group(req.target_id, "operator") ||
        permissions_->has_command_group(req.target_id, "admin")) {
        return core::fail(BanFromChannelError::NotAuthorized);
    }

    // 6. Check if target already banned
    if (channel.is_banned(req.target_id)) {
        return core::fail(BanFromChannelError::TargetAlreadyBanned);
    }

    // 7. Call channel.kick() — this also adds to banlist
    bool kicked = channel.kick(req.banner_id, req.target_id);
    if (!kicked) {
        // Banner not actually a member
        return core::fail(BanFromChannelError::InsufficientPermissions);
    }

    // 8. Save updated channel
    auto save_result = channels_->save(channel);
    if (!save_result) {
        return core::fail(BanFromChannelError::ChannelNotFound);
    }

    // 9. Drain and route events
    auto events = channel.drain_events();
    // Events would be routed here via router_
    // For now, just acknowledge success

    return {};
}

}  // namespace pvpgn::application::chat
