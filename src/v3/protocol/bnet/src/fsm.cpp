// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/bnet/fsm.hpp"

#include <variant>

#include "core/error.hpp"

namespace pvpgn::protocol::bnet {

core::Status<> BnetFsm::reject(const char* reason) {
    state_ = BnetState::Closing;
    ctx_->close();
    return core::fail(core::Error{core::StatusCode::InvalidArgument, reason});
}

core::Status<> BnetFsm::handle(const ClientMessage& msg) {
    if (state_ == BnetState::Closing) {
        return core::fail(core::Error{
            core::StatusCode::FailedPrecondition, "bnet fsm: closing"});
    }
    return std::visit([this](const auto& m) { return on(m); }, msg);
}

core::Status<> BnetFsm::on(const Null&) {
    // Keepalive in every state. No reply required.
    return core::ok();
}

core::Status<> BnetFsm::on(const Ping& p) {
    // Mirror the cookie verbatim; this is the canonical ECHOREPLY.
    return ctx_->send(ServerMessage{Ping{p.ticks}});
}

core::Status<> BnetFsm::on(const AuthInfo&) {
    if (state_ != BnetState::Init) {
        return reject("bnet fsm: AUTH_INFO out of order");
    }
    state_ = BnetState::AuthInfoReceived;
    // Phase-5 use-case will plug version-check policy here. For now we
    // ack with result=0 ("passed") + empty info so a basic client can
    // proceed and exercise downstream states.
    return ctx_->send(ServerMessage{AuthCheckReply{0u, ""}});
}

core::Status<> BnetFsm::on(const LogonResponse2& m) {
    if (state_ != BnetState::AuthInfoReceived) {
        return reject("bnet fsm: LOGONRESPONSE2 out of order");
    }
    // Phase-5 will look up the account / verify the hash. Skeleton
    // accepts any non-empty username.
    if (m.username.empty()) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x01u, ""}});
    }
    state_ = BnetState::LoggedIn;
    return ctx_->send(ServerMessage{LogonResponse2Reply{0x00u, ""}});
}

core::Status<> BnetFsm::on(const EnterChatRequest& m) {
    if (state_ != BnetState::LoggedIn && state_ != BnetState::InChat) {
        return reject("bnet fsm: ENTERCHAT out of order");
    }
    state_ = BnetState::InChat;
    // Echo a synthetic reply; the real handler will populate fields.
    return ctx_->send(ServerMessage{EnterChatReply{
        /*unique_name*/ m.username,
        /*statstring*/  m.statstring,
        /*account*/     m.username}});
}

core::Status<> BnetFsm::on(const JoinChannel& m) {
    if (state_ != BnetState::InChat) {
        return reject("bnet fsm: JOINCHANNEL before ENTERCHAT");
    }
    // Phase 5 dispatches to the channel use-case which will replay the
    // member list as a CHATEVENT stream. Skeleton emits a single
    // "show user in channel" event so observers can prove the wire
    // path. Event id 7 = EID_CHANNEL.
    return ctx_->send(ServerMessage{ChatEvent{
        /*event_id*/    7,
        /*flags*/       0,
        /*ping_ms*/     0,
        /*user_ip*/     0,
        /*acct_number*/ 0,
        /*registration*/0,
        /*username*/    "",
        /*text*/        m.channel}});
}

core::Status<> BnetFsm::on(const ChatCommand& m) {
    if (state_ != BnetState::InChat) {
        return reject("bnet fsm: CHATCOMMAND outside chat");
    }
    // Phase 5: dispatch to chat / command use-case. Skeleton: echo as
    // EID_TALK (event 5) attributed to "<self>".
    return ctx_->send(ServerMessage{ChatEvent{
        /*event_id*/    5,
        /*flags*/       0,
        /*ping_ms*/     0,
        /*user_ip*/     0,
        /*acct_number*/ 0,
        /*registration*/0,
        /*username*/    "<self>",
        /*text*/        m.text}});
}

}  // namespace pvpgn::protocol::bnet
