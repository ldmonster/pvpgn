// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file op_from_channel.hpp
/// OP_FROM_CHANNEL use-case — grant or revoke operator status in a channel.
///
/// Validates that the requester holds operator permission in the channel,
/// resolves the target account by name, confirms the target is a channel
/// member, then grants or revokes operator status via the channel aggregate.

#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "application/ports/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur when granting/revoking channel operator status.
enum class OpFromChannelError : std::uint8_t {
    ChannelNotFound,
    TargetNotFound,       ///< Target account not found or not in channel.
    PermissionDenied,     ///< Requester is not a channel operator.
};

/// Command to grant or revoke operator status in a channel.
struct OpFromChannelCommand {
    domain::AccountId requester_id;
    domain::ChannelId channel_id;
    std::string       target_name;  ///< Account name to op/deop.
    bool              grant;        ///< true = grant op, false = revoke op.
};

class OpFromChannel {
public:
    explicit OpFromChannel(
        std::shared_ptr<application::ports::IChannelRepository> channels,
        std::shared_ptr<application::ports::IAccountRepository> accounts,
        std::shared_ptr<application::ports::IPermissionChecker> permissions)
        : channels_(channels), accounts_(accounts), permissions_(permissions) {}

    /// Execute: check permissions, resolve target, and update op status.
    /// Returns OpFromChannelError::PermissionDenied if requester is not operator.
    /// Returns OpFromChannelError::TargetNotFound if target is not in channel.
    [[nodiscard]] core::Result<void, OpFromChannelError>
    execute(OpFromChannelCommand cmd) const;

private:
    std::shared_ptr<application::ports::IChannelRepository> channels_;
    std::shared_ptr<application::ports::IAccountRepository> accounts_;
    std::shared_ptr<application::ports::IPermissionChecker> permissions_;
};

}  // namespace pvpgn::application::chat
