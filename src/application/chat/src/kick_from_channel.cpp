// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/kick_from_channel.hpp"

#include "domain/chat/ports.hpp"
#include "domain/chat/channel.hpp"
#include "domain/moderation/ports.hpp"

namespace pvpgn::application::chat {

core::Result<void, KickFromChannelError>
KickFromChannel::execute(const KickFromChannelRequest& req) const {
    // 1. Validate not self-kick
    if (req.kicker_id == req.target_id) {
        return core::fail(KickFromChannelError::CannotKickSelf);
    }

    // 2. Authorize the kicker. The original `_handle_kick_command` requires
    //    the actor to be a channel admin / operator / tempOP; here we gate on
    //    the same "operator" command group used by OpFromChannel, which is the
    //    only per-actor authorization seam currently exposed.
    if (!permissions_->has_command_group(req.kicker_id, "operator")) {
        return core::fail(KickFromChannelError::NotAuthorized);
    }

    // 3. Find the channel
    auto found = channels_->find_by_id(req.channel_id);
    if (!found) {
        return core::fail(KickFromChannelError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 4. Verify kicker and target are members
    if (!channel.contains(req.kicker_id)) {
        return core::fail(KickFromChannelError::TargetNotInChannel);
    }

    if (!channel.contains(req.target_id)) {
        return core::fail(KickFromChannelError::TargetNotInChannel);
    }

    // 5. Operator/admin immunity: the original refuses to kick administrators
    //    or operators. Reject if the target holds either group.
    if (permissions_->has_command_group(req.target_id, "operator") ||
        permissions_->has_command_group(req.target_id, "admin")) {
        return core::fail(KickFromChannelError::NotAuthorized);
    }

    // 6. Call channel.kick() — domain mechanically removes + banlists.
    bool kicked = channel.kick(req.kicker_id, req.target_id);
    if (!kicked) {
        // Kicker not actually a member (double-check)
        return core::fail(KickFromChannelError::InsufficientPermissions);
    }

    // 7. Save updated channel
    auto save_result = channels_->save(channel);
    if (!save_result) {
        return core::fail(KickFromChannelError::ChannelNotFound);
    }

    // 8. Drain and route events
    auto events = channel.drain_events();
    // Events would be routed here via router_
    // For now, just acknowledge success

    return {};
}

}  // namespace pvpgn::application::chat
