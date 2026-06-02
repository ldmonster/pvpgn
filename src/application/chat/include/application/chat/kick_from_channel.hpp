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

namespace pvpgn::application::chat {

/// Errors that can occur during kick.
enum class KickFromChannelError : std::uint8_t {
    ChannelNotFound,
    TargetNotInChannel,
    InsufficientPermissions,  // kicker rank <= target rank
    CannotKickSelf,
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
        std::shared_ptr<domain::identity::IAccountRepository> accounts,
        std::shared_ptr<domain::connection::IMessageRouter> router)
        : channels_(channels), accounts_(accounts), router_(router) {}

    /// Execute: validate permissions and kick member from channel.
    core::Result<void, KickFromChannelError>
    execute(const KickFromChannelRequest& req) const;

private:
    std::shared_ptr<domain::chat::IChannelRepository> channels_;
    std::shared_ptr<domain::identity::IAccountRepository> accounts_;
    std::shared_ptr<domain::connection::IMessageRouter>     router_;
};

}  // namespace pvpgn::application::chat
