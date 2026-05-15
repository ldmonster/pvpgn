// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/ban_from_channel.hpp"

#include "application/ports/channel_repository.hpp"
#include "domain/chat/channel.hpp"

namespace pvpgn::application::chat {

core::Result<void, BanFromChannelError>
BanFromChannel::execute(const BanFromChannelRequest& req) const {
    // 1. Validate not self-ban
    if (req.banner_id == req.target_id) {
        return core::fail(BanFromChannelError::CannotBanSelf);
    }

    // 2. Find the channel
    auto found = channels_->find_by_id(req.channel_id);
    if (!found) {
        return core::fail(BanFromChannelError::ChannelNotFound);
    }

    domain::chat::Channel channel = found.value();

    // 3. Verify banner is a member (permission check)
    if (!channel.contains(req.banner_id)) {
        return core::fail(BanFromChannelError::InsufficientPermissions);
    }

    // 4. Check if target already banned
    if (channel.is_banned(req.target_id)) {
        return core::fail(BanFromChannelError::TargetAlreadyBanned);
    }

    // 5. Call channel.kick() — this also adds to banlist
    bool kicked = channel.kick(req.banner_id, req.target_id);
    if (!kicked) {
        // Banner not actually a member
        return core::fail(BanFromChannelError::InsufficientPermissions);
    }

    // 6. Save updated channel
    auto save_result = channels_->save(channel);
    if (!save_result) {
        return core::fail(BanFromChannelError::ChannelNotFound);
    }

    // 7. Drain and route events
    auto events = channel.drain_events();
    // Events would be routed here via router_
    // For now, just acknowledge success

    return {};
}

}  // namespace pvpgn::application::chat
