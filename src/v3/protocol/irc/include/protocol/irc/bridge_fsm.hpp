// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bridge_fsm.hpp
/// IRC bridge FSM that connects IRC channels to PvPGN chat use-cases.
///
/// `IrcBridgeFsm` is a thin factory/wiring wrapper around `IrcFsm`.
/// It bundles all required use-case pointers into a single `UseCaseContext`
/// struct and constructs the underlying FSM with the full R301 constructor.
///
/// Channel lifecycle (R301):
///   JOIN  → JoinChannel::execute()  → 332 RPL_TOPIC + 353 RPL_NAMREPLY + 366
///   PART  → LeaveChannel::execute() → PART echo
///   PRIVMSG #chan → PostMessage::execute()
///   LIST  → ListChannels::execute() → 321 + 322... + 323
///
/// Name mapping helpers:
///   irc_to_bnet_channel("#Lobby") → "Lobby"
///   bnet_to_irc_channel("Lobby")  → "#Lobby"

#include <memory>
#include <string>
#include <string_view>

#include "protocol/irc/fsm.hpp"

// Forward declarations — avoid pulling in all use-case headers.
namespace pvpgn::application::auth {
class LoginUser;
class LogoutUser;
}  // namespace pvpgn::application::auth

namespace pvpgn::application::chat {
class JoinChannel;
class LeaveChannel;
class ListChannels;
class PostMessage;
}  // namespace pvpgn::application::chat

namespace pvpgn::protocol::irc {

/// IRC FSM with channel bridging to PvPGN chat use-cases (R301).
class IrcBridgeFsm : public IrcFsm {
public:
    /// All use-cases needed by the bridge FSM.
    struct UseCaseContext {
        /// OLS authentication use-case (may be null for skeleton/test mode).
        application::auth::LoginUser*                            login_user    = nullptr;
        /// Logout use-case (reserved for Phase H; may be null).
        application::auth::LogoutUser*                           logout_user   = nullptr;
        /// Chat use-cases (R301). Null = stub mode for that command.
        std::shared_ptr<application::chat::ListChannels>         list_channels;
        std::shared_ptr<application::chat::JoinChannel>          join_channel;
        std::shared_ptr<application::chat::PostMessage>          post_message;
        std::shared_ptr<application::chat::LeaveChannel>         leave_channel;
    };

    /// Construct with a session context and wired use-cases.
    /// @param ctx       Session I/O context. Shared ownership; must outlive FSM.
    /// @param use_cases Bundle of use-case pointers (nulls = stub mode).
    IrcBridgeFsm(std::shared_ptr<ISessionContext> ctx,
                 UseCaseContext use_cases)
        : IrcFsm(*ctx,
                 use_cases.login_user,
                 std::move(use_cases.list_channels),
                 std::move(use_cases.join_channel),
                 std::move(use_cases.post_message),
                 std::move(use_cases.leave_channel))
        , ctx_owner_(std::move(ctx)) {}

    // ---- Name-mapping helpers -----------------------------------------------

    /// Map IRC channel name to BNet/domain channel name.
    /// "#Lobby" → "Lobby"
    [[nodiscard]] static std::string irc_to_bnet_channel(std::string_view irc_channel);

    /// Map BNet/domain channel name to IRC channel name.
    /// "Lobby" → "#Lobby"
    [[nodiscard]] static std::string bnet_to_irc_channel(std::string_view bnet_channel);

private:
    /// Keep the context alive for the lifetime of the FSM.
    std::shared_ptr<ISessionContext> ctx_owner_;
};

}  // namespace pvpgn::protocol::irc
