// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/kick_from_channel.hpp"

#include "application/ports/channel_repository.hpp"
#include "domain/chat/channel.hpp"

namespace pvpgn::application::chat {

core::Result<void, KickFromChannelError>
KickFromChannel::execute(const KickFromChannelRequest& req) const {
    // 1. Validate not self-kick
    if (req.kicker_id == req.target_id) {
        return core::fail(KickFromChannelError::CannotKickSelf);
    }

    // 2. Find the channel
    auto found = channels_->find_by_id(req.channel_id);
    if (!found) {
        return core::fail(KickFromChannelError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 3. Verify kicker and target are members
    if (!channel.contains(req.kicker_id)) {
        return core::fail(KickFromChannelError::TargetNotInChannel);
    }

    if (!channel.contains(req.target_id)) {
        return core::fail(KickFromChannelError::TargetNotInChannel);
    }

    // 4. Call channel.kick() — domain enforces permissions if needed
    bool kicked = channel.kick(req.kicker_id, req.target_id);
    if (!kicked) {
        // Kicker not actually a member (double-check)
        return core::fail(KickFromChannelError::InsufficientPermissions);
    }

    // 5. Save updated channel
    auto save_result = channels_->save(channel);
    if (!save_result) {
        return core::fail(KickFromChannelError::ChannelNotFound);
    }

    // 6. Drain and route events
    auto events = channel.drain_events();
    // Events would be routed here via router_
    // For now, just acknowledge success

    return {};
}

}  // namespace pvpgn::application::chat
