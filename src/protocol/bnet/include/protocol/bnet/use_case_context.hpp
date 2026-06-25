// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file use_case_context.hpp
/// Bundle of use-case dependencies for BnetFsm.
/// Injected into BnetFsm to enable wiring of protocol handlers to domain operations.

#include <memory>

#include "domain/identity/ports.hpp"
#include "domain/connection/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/chat/ports/command_registry.hpp"

// Forward declarations
namespace pvpgn::application::auth {
class LoginUser;
class CreateAccount;
class ChangePassword;
class LoginUserW3;
class ISrp3CredentialStore;
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

namespace pvpgn::protocol::bnet {

/// Aggregates all use-case dependencies needed by BnetFsm handlers.
/// All pointers are non-null; check in factory before passing.
struct BnetUseCaseContext {
    std::shared_ptr<application::auth::LoginUser> login_user;
    std::shared_ptr<application::auth::CreateAccount> create_account;
    std::shared_ptr<application::auth::ChangePassword> change_password;
    std::shared_ptr<application::chat::JoinChannel> join_channel;
    std::shared_ptr<application::chat::PostMessage> post_message;
    std::shared_ptr<application::chat::LeaveChannel> leave_channel;
    std::shared_ptr<application::chat::ListChannels> list_channels;
    std::shared_ptr<application::game::StartGame> start_game;
    std::shared_ptr<application::game::JoinGame> join_game;
    std::shared_ptr<application::game::LeaveGame> leave_game;
    std::shared_ptr<application::moderation::CheckIpBan> check_ip_ban;
    std::shared_ptr<domain::identity::IAccountRepository> account_repo;
    std::shared_ptr<application::ports::ICommandRegistry> command_registry;
    std::shared_ptr<domain::connection::IMessageRouter> message_router;
    std::shared_ptr<domain::moderation::IPermissionChecker> permission_checker;
    std::shared_ptr<domain::identity::ISessionRegistry> session_registry;
    /// WarCraft III SRP-3 login (SID_AUTH_ACCOUNTLOGON/PROOF) + its credential
    /// store (written on SID_AUTH_ACCOUNTCREATE). Null for non-W3 deployments.
    std::shared_ptr<application::auth::LoginUserW3> login_user_w3;
    std::shared_ptr<application::auth::ISrp3CredentialStore> srp3_store;
};

}  // namespace pvpgn::protocol::bnet
