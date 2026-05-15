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

namespace pvpgn::application::ports {
class IChannelRepository;
class IAccountRepository;
class IMessageRouter;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::chat {

/// Errors that can occur during ban.
enum class BanFromChannelError : std::uint8_t {
    ChannelNotFound,
    TargetAlreadyBanned,
    InsufficientPermissions,
    CannotBanSelf,
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
        std::shared_ptr<application::ports::IChannelRepository> channels,
        std::shared_ptr<application::ports::IAccountRepository> accounts,
        std::shared_ptr<application::ports::IMessageRouter> router)
        : channels_(channels), accounts_(accounts), router_(router) {}

    /// Execute: validate permissions and ban member from channel.
    core::Result<void, BanFromChannelError>
    execute(const BanFromChannelRequest& req) const;

private:
    std::shared_ptr<application::ports::IChannelRepository> channels_;
    std::shared_ptr<application::ports::IAccountRepository> accounts_;
    std::shared_ptr<application::ports::IMessageRouter>     router_;
};

}  // namespace pvpgn::application::chat
