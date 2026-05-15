// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bridge_fsm.hpp
/// IRC bridge FSM that connects IRC channels to BNet use-cases.
/// Extends IrcFsm with real use-case bindings.

#include <memory>
#include <string>
#include <string_view>

#include "protocol/irc/fsm.hpp"

// Forward declarations
namespace pvpgn::application::chat {
class JoinChannel;
class PostMessage;
class LeaveChannel;
}
namespace pvpgn::application::auth {
class LoginUser;
class LogoutUser;
}

namespace pvpgn::protocol::irc {

/// IRC FSM with channel bridging to BNet use-cases.
class IrcBridgeFsm : public IrcFsm {
public:
    struct UseCaseContext {
        std::shared_ptr<pvpgn::application::chat::JoinChannel> join_channel;
        std::shared_ptr<pvpgn::application::chat::PostMessage> post_message;
        std::shared_ptr<pvpgn::application::chat::LeaveChannel> leave_channel;
        std::shared_ptr<pvpgn::application::auth::LoginUser> login_user;
        std::shared_ptr<pvpgn::application::auth::LogoutUser> logout_user;
    };

    IrcBridgeFsm(std::shared_ptr<ISessionContext> ctx,
                 UseCaseContext use_cases)
        : IrcFsm(*ctx), ctx_(ctx), use_cases_(use_cases) {}

    /// Map IRC channel name to BNet channel name.
    /// #ChannelName → "ChannelName"
    static std::string irc_to_bnet_channel(std::string_view irc_channel);

    /// Map BNet channel name to IRC channel name.
    /// "ChannelName" → #ChannelName
    static std::string bnet_to_irc_channel(std::string_view bnet_channel);

private:
    std::shared_ptr<ISessionContext> ctx_;
    UseCaseContext use_cases_;
};

}  // namespace pvpgn::protocol::irc
