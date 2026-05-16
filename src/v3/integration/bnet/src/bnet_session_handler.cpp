// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/bnet/bnet_session_handler.hpp"

#include "application/chat/join_channel.hpp"
#include "application/chat/post_message.hpp"
#include "application/auth/login_user.hpp"
#include "application/auth/logout_user.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/shared/chat_message.hpp"

namespace pvpgn::integration::bnet {

BnetSessionHandler::BnetSessionHandler(Dependencies deps, std::string session_id)
    : deps_(deps), session_id_(std::move(session_id)) {}

core::Result<void, core::Error> BnetSessionHandler::on_join_channel(
    std::string_view channel_name, uint32_t flags) {
    
    if (!logged_in_) {
        return core::fail(core::Error(
            core::StatusCode::Unauthenticated,
            "Cannot join channel: not logged in"
        ));
    }
    
    if (!deps_.join_channel) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "JoinChannel use case not available"
        ));
    }
    
    // Store current channel for later reference
    current_channel_ = std::string(channel_name);
    
    // The actual channel join would be delegated to the use case
    // For now, we just record the state transition
    return core::Result<void, core::Error>{};
}

core::Result<void, core::Error> BnetSessionHandler::on_chat_command(std::string_view text) {
    if (!logged_in_) {
        return core::fail(core::Error(
            core::StatusCode::Unauthenticated,
            "Cannot send message: not logged in"
        ));
    }
    
    if (current_channel_.empty()) {
        return core::fail(core::Error(
            core::StatusCode::FailedPrecondition,
            "Cannot send message: not in a channel"
        ));
    }
    
    if (!deps_.post_message) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "PostMessage use case not available"
        ));
    }
    
    // The actual message posting would be delegated to the use case
    // For now, we just validate the state
    return core::Result<void, core::Error>{};
}

core::Result<void, core::Error> BnetSessionHandler::on_login(
    std::string_view account_name, std::string_view password_hash) {
    
    if (!deps_.login_user) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "LoginUser use case not available"
        ));
    }
    
    // Store account name for session tracking
    account_name_ = std::string(account_name);
    logged_in_ = true;
    
    // The actual login validation would be delegated to the use case
    // For now, we just record the state transition
    return core::Result<void, core::Error>{};
}

core::Result<void, core::Error> BnetSessionHandler::on_disconnect() {
    if (logged_in_ && deps_.logout_user) {
        // Delegate logout to use case
        logged_in_ = false;
    }
    
    account_name_.clear();
    current_channel_.clear();
    
    return core::Result<void, core::Error>{};
}

}  // namespace pvpgn::integration::bnet
