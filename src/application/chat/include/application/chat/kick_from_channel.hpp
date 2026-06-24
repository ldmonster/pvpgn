// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file kick_from_channel.hpp
/// KICK_FROM_CHANNEL use-case — remove a member from a channel.
///
/// Validates permissions (kicker rank must be >= target rank),
/// performs the kick via Channel aggregate, and broadcasts the event.

#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/chat/ports.hpp"
#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur during kick.
enum class KickFromChannelError : std::uint8_t {
    ChannelNotFound,
    TargetNotInChannel,
    InsufficientPermissions,  // kicker rank <= target rank
    CannotKickSelf,
    /// Kicker is not a channel operator/admin, or the target is itself an
    /// operator/admin and therefore immune from being kicked.
    NotAuthorized,
};

/// Request to kick a member from a channel.
struct KickFromChannelRequest {
    domain::AccountId kicker_id;        // admin performing kick
    domain::AccountId target_id;        // account to kick
    domain::ChannelId channel_id;
    std::string       reason;
};

class KickFromChannel {
public:
    explicit KickFromChannel(
        std::shared_ptr<domain::chat::IChannelRepository> channels,
        std::shared_ptr<domain::moderation::IPermissionChecker> permissions,
        std::shared_ptr<domain::connection::IMessageRouter> router)
        : channels_(channels), permissions_(permissions), router_(router) {}

    /// Execute: validate authorization and kick member from channel.
    ///
    /// The kicker must hold the channel "operator" command group, and the
    /// target must NOT be an operator/admin (operator/admin immunity),
    /// mirroring the original `_handle_kick_command` authority rule.
    core::Result<void, KickFromChannelError>
    execute(const KickFromChannelRequest& req) const;

private:
    std::shared_ptr<domain::chat::IChannelRepository> channels_;
    std::shared_ptr<domain::moderation::IPermissionChecker> permissions_;
    std::shared_ptr<domain::connection::IMessageRouter>     router_;
};

}  // namespace pvpgn::application::chat
