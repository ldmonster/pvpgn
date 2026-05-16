// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnet_session_handler.hpp
/// Wires BNet protocol events to application use cases.
/// This is the "anti-corruption layer" between protocol and domain.

#include "core/result.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/post_message.hpp"
#include "application/auth/login_user.hpp"
#include "application/auth/logout_user.hpp"
#include <memory>
#include <string>
#include <functional>

namespace pvpgn::integration::bnet {

// Forward declarations
namespace application::chat {
class JoinChannel;
class PostMessage;
}  // namespace application::chat

namespace application::auth {
class LoginUser;
class LogoutUser;
}  // namespace application::auth

// Wires BNet protocol events to application use cases
// This is the "anti-corruption layer" between protocol and domain
class BnetSessionHandler {
public:
    struct Dependencies {
        application::chat::JoinChannel* join_channel;
        application::chat::PostMessage* post_message;
        application::auth::LoginUser* login_user;
        application::auth::LogoutUser* logout_user;
    };
    
    explicit BnetSessionHandler(Dependencies deps, std::string session_id);
    
    // Called by FSM when SID_JOINCHANNEL (0x0C) is received
    core::Result<void, core::Error> on_join_channel(std::string_view channel_name, uint32_t flags);
    
    // Called by FSM when SID_CHATCOMMAND (0x0E) is received
    core::Result<void, core::Error> on_chat_command(std::string_view text);
    
    // Called by FSM when SID_LOGONRESPONSE2 (0x3A) is received
    core::Result<void, core::Error> on_login(std::string_view account_name, std::string_view password_hash);
    
    // Called by FSM when connection closes
    core::Result<void, core::Error> on_disconnect();
    
    const std::string& session_id() const noexcept { return session_id_; }
    const std::string& account_name() const noexcept { return account_name_; }
    const std::string& current_channel() const noexcept { return current_channel_; }

private:
    Dependencies deps_;
    std::string session_id_;
    std::string account_name_;
    std::string current_channel_;
    bool logged_in_ = false;
};

}  // namespace pvpgn::integration::bnet
