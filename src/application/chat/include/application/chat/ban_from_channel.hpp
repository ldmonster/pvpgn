// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ban_from_channel.hpp
/// BAN_FROM_CHANNEL use-case — ban a member from a channel.
///
/// Validates permissions, adds to channel ban list, and kicks if present.
/// Broadcasts ban notification to remaining members.

#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/chat/ports.hpp"
#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur during ban.
enum class BanFromChannelError : std::uint8_t {
    ChannelNotFound,
    TargetAlreadyBanned,
    InsufficientPermissions,
    CannotBanSelf,
    /// Banner is not a channel operator/admin, or the target is itself an
    /// operator/admin and therefore immune from being banned.
    NotAuthorized,
};

/// Request to ban a member from a channel.
struct BanFromChannelRequest {
    domain::AccountId banner_id;
    domain::AccountId target_id;
    domain::UserName  target_name;
    domain::ChannelId channel_id;
    std::string       reason;
};

class BanFromChannel {
public:
    explicit BanFromChannel(
        std::shared_ptr<domain::chat::IChannelRepository> channels,
        std::shared_ptr<domain::moderation::IPermissionChecker> permissions,
        std::shared_ptr<domain::connection::IMessageRouter> router)
        : channels_(channels), permissions_(permissions), router_(router) {}

    /// Execute: validate authorization and ban member from channel.
    ///
    /// The banner must hold the channel "operator" command group, and the
    /// target must NOT be an operator/admin (operator/admin immunity),
    /// mirroring the original `_handle_ban_command` authority rule.
    core::Result<void, BanFromChannelError>
    execute(const BanFromChannelRequest& req) const;

private:
    std::shared_ptr<domain::chat::IChannelRepository> channels_;
    std::shared_ptr<domain::moderation::IPermissionChecker> permissions_;
    std::shared_ptr<domain::connection::IMessageRouter>     router_;
};

}  // namespace pvpgn::application::chat
