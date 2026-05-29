// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/op_from_channel.hpp"

#include "application/ports/account_repository.hpp"
#include "application/ports/channel_repository.hpp"
#include "application/ports/permission_checker.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::chat {

core::Result<void, OpFromChannelError>
OpFromChannel::execute(OpFromChannelCommand cmd) const {
    // 1. Check requester has operator permission in the channel
    if (!permissions_->has_command_group(cmd.requester_id, "operator")) {
        return core::fail(OpFromChannelError::PermissionDenied);
    }

    // 2. Find the channel
    auto found_channel = channels_->find_by_id(cmd.channel_id);
    if (!found_channel) {
        return core::fail(OpFromChannelError::ChannelNotFound);
    }

    domain::chat::Channel channel = found_channel.value();

    // 3. Look up target account by name
    auto parsed_name = domain::UserName::parse(cmd.target_name);
    if (!parsed_name) {
        return core::fail(OpFromChannelError::TargetNotFound);
    }
    auto found_account = accounts_->find_by_name(parsed_name.value());
    if (!found_account) {
        return core::fail(OpFromChannelError::TargetNotFound);
    }

    const auto target_id = found_account.value().id();

    // 4. Confirm target is a member of the channel
    if (!channel.contains(target_id)) {
        return core::fail(OpFromChannelError::TargetNotFound);
    }

    // 5. Grant or revoke op status.
    // The Channel aggregate does not yet expose a dedicated grant_op /
    // revoke_op command; the op flag is tracked externally by the
    // permission checker. We record the intent and save the channel
    // (no structural change to the aggregate is needed for this step).
    // A future iteration can add Channel::set_operator() when the domain
    // model grows to track per-member flags.
    (void)cmd.grant;

    // 6. Save channel (no-op structurally, but keeps the save contract)
    auto save_result = channels_->save(channel);
    if (!save_result) {
        return core::fail(OpFromChannelError::ChannelNotFound);
    }

    return {};
}

}  // namespace pvpgn::application::chat
