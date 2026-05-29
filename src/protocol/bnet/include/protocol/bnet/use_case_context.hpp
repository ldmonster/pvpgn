// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file use_case_context.hpp
/// Bundle of use-case dependencies for BnetFsm.
/// Injected into BnetFsm to enable wiring of protocol handlers to domain operations.

#include <memory>

// Forward declarations
namespace pvpgn::application::auth {
class LoginUser;
class ChangePassword;
}  // namespace pvpgn::application::auth

namespace pvpgn::application::chat {
class JoinChannel;
class PostMessage;
class LeaveChannel;
class ListChannels;
}  // namespace pvpgn::application::chat

namespace pvpgn::application::game {
class StartGame;
class JoinGame;
class LeaveGame;
}  // namespace pvpgn::application::game

namespace pvpgn::application::moderation {
class CheckIpBan;
}  // namespace pvpgn::application::moderation

namespace pvpgn::application::ports {
class IAccountRepository;
class ICommandRegistry;
class IMessageRouter;
class IPermissionChecker;
class ISessionRegistry;
}  // namespace pvpgn::application::ports

namespace pvpgn::protocol::bnet {

/// Aggregates all use-case dependencies needed by BnetFsm handlers.
/// All pointers are non-null; check in factory before passing.
struct BnetUseCaseContext {
    std::shared_ptr<application::auth::LoginUser> login_user;
    std::shared_ptr<application::auth::ChangePassword> change_password;
    std::shared_ptr<application::chat::JoinChannel> join_channel;
    std::shared_ptr<application::chat::PostMessage> post_message;
    std::shared_ptr<application::chat::LeaveChannel> leave_channel;
    std::shared_ptr<application::chat::ListChannels> list_channels;
    std::shared_ptr<application::game::StartGame> start_game;
    std::shared_ptr<application::game::JoinGame> join_game;
    std::shared_ptr<application::game::LeaveGame> leave_game;
    std::shared_ptr<application::moderation::CheckIpBan> check_ip_ban;
    std::shared_ptr<application::ports::IAccountRepository> account_repo;
    std::shared_ptr<application::ports::ICommandRegistry> command_registry;
    std::shared_ptr<application::ports::IMessageRouter> message_router;
    std::shared_ptr<application::ports::IPermissionChecker> permission_checker;
    std::shared_ptr<application::ports::ISessionRegistry> session_registry;
};

}  // namespace pvpgn::protocol::bnet
