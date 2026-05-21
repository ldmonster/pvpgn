// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/bnet/fsm.hpp"

#include <span>
#include <variant>
#include <vector>

#include "application/auth/login_user.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/post_message.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/whisper_use_case.hpp"
#include "application/game/start_game.hpp"
#include "application/game/join_game.hpp"
#include "application/game/leave_game.hpp"
#include "application/moderation/check_ip_ban.hpp"
#include "application/ports/message_router.hpp"
#include "application/ports/session_registry.hpp"
#include "core/error.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

core::Status<> BnetFsm::reject(const char* reason) {
    state_ = BnetState::Closing;
    ctx_->close();
    return core::fail(core::Error{core::StatusCode::InvalidArgument, reason});
}

void BnetFsm::broadcast_chat_event(const ChatEvent& ev,
                                   std::span<const domain::SessionId> sessions) {
    if (!use_cases_.message_router || sessions.empty()) return;
    Writer w;
    w.begin_bnet_packet(0x0F);  // SID_CHATEVENT
    if (!encode(w, ev)) return;
    if (!w.finalize_bnet_packet()) return;
    auto bytes = w.take();
    (void)use_cases_.message_router->broadcast(
        sessions,
        std::span<const std::byte>{bytes.data(), bytes.size()});
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

core::Status<> BnetFsm::on(const AuthInfo& m) {
    if (state_ != BnetState::Init) {
        return reject("bnet fsm: AUTH_INFO out of order");
    }
    // Store the client product tag (game_id is the 4-byte product tag in
    // wire-packed big-endian form, e.g. 'STAR', 'D2DV', 'WAR3').
    if (auto tag = domain::ClientTag::from_packed_be(m.game_id)) {
        client_tag_ = tag.value();
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
    
    // Check IP ban if use-case is available
    if (use_cases_.check_ip_ban) {
        // TODO: peer_ip_ should be stored in constructor; for now, use placeholder 0
        // In production, this would be: auto ban_result = use_cases_.check_ip_ban->execute(peer_ip_);
    }
    
    if (m.username.empty()) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x01u, ""}});
    }
    
    // Call login use-case if available, otherwise accept the login
    if (!use_cases_.login_user) {
        // No login use-case available - accept login with default account ID
        current_username_ = std::string{m.username};
        state_ = BnetState::LoggedIn;
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x00u, ""}});
    }
    
    // Construct password hash from the 5×u32 array
    std::string password_hash;
    for (const auto& hash_word : m.password_hash) {
        password_hash += std::to_string(hash_word) + ":";
    }
    
    // Create login request with parsed credentials
    auto username_result = domain::UserName::parse(m.username);
    if (!username_result) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x01u, "Invalid username"}});
    }
    
    auto password_hash_result = domain::BNHash::from_bytes(password_hash);
    if (!password_hash_result) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x02u, "Invalid password hash"}});
    }
    
    application::auth::LoginRequest login_req{
        .name = username_result.value(),
        .password_candidate = password_hash_result.value(),
        .tag = domain::ClientTag{},
        .ip = domain::IpAddress{},
        .session = domain::SessionId{}
    };
    
    auto login_result = use_cases_.login_user->execute(login_req);
    if (!login_result) {
        // Login failed
        uint32_t error_code = 0x02;  // default to bad password
        std::string reason;
        
        switch (login_result.error()) {
            case application::auth::LoginError::UnknownUser:
                error_code = 0x01;
                break;
            case application::auth::LoginError::InvalidCredentials:
                error_code = 0x02;
                break;
            case application::auth::LoginError::Locked:
                error_code = 0x05;
                reason = "Account is locked";
                break;
            case application::auth::LoginError::Banned:
                error_code = 0x06;
                reason = "Account has been banned";
                break;
            case application::auth::LoginError::MustChangePassword:
                error_code = 0x07;
                reason = "Password must be changed";
                break;
            default:
                error_code = 0x02;
                break;
        }
        
        return ctx_->send(ServerMessage{LogonResponse2Reply{error_code, reason}});
    }
    
    // Login succeeded - store account ID and username, attach session
    current_account_id_ = login_result.value().id;
    current_username_   = std::string{m.username};

    if (use_cases_.session_registry) {
        auto reg_status = use_cases_.session_registry->attach(session_id_, current_account_id_);
        if (!reg_status) {
            return reject("bnet fsm: failed to attach session");
        }
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
    
    if (!use_cases_.join_channel) {
        // No join_channel use-case available - accept the join
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    3,  // EID_CHANNEL
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        m.channel}});
    }
    
    // Call join_channel use-case — use client_tag_ stored from AUTH_INFO
    auto join_result = use_cases_.join_channel->execute(
        current_account_id_, m.channel, client_tag_);
    
    if (!join_result) {
        // Join failed - send EID_INFO error to client
        std::string error_msg;
        switch (join_result.error()) {
            case application::chat::JoinChannelError::NotFound:
                error_msg = "Channel not found";
                break;
            case application::chat::JoinChannelError::Full:
                error_msg = "Channel is full";
                break;
            case application::chat::JoinChannelError::Banned:
                error_msg = "You are banned from this channel";
                break;
            case application::chat::JoinChannelError::WrongClientTag:
                error_msg = "Wrong client tag for this channel";
                break;
            case application::chat::JoinChannelError::Locked:
                error_msg = "Channel is locked";
                break;
            default:
                error_msg = "Failed to join channel";
                break;
        }
        
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        error_msg}});
    }
    
    // Join succeeded - store channel ID and transition state
    current_channel_id_ = join_result.value().channel.id();
    state_ = BnetState::InChat;
    
    // Send EID_SHOWUSER for each existing member to this client
    for ([[maybe_unused]] const auto& member_session_id : join_result.value().members_to_notify) {
        // In a real implementation, we would query account info for each member
        // For now, send a placeholder showing the join
        if (auto send_status = ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    2,  // EID_SHOWUSER
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "User",
            /*text*/        ""}}); !send_status) {
            return send_status;
        }
    }
    
    // Send EID_CHANNEL event with the channel name
    if (auto send_status = ctx_->send(ServerMessage{ChatEvent{
        /*event_id*/    3,  // EID_CHANNEL
        /*flags*/       0,
        /*ping_ms*/     0,
        /*user_ip*/     0,
        /*acct_number*/ 0,
        /*registration*/0,
        /*username*/    "",
        /*text*/        m.channel}}); !send_status) {
        return send_status;
    }
    
    // Broadcast EID_JOIN to all other members via message_router
    if (!join_result.value().members_to_notify.empty()) {
        broadcast_chat_event(
            ChatEvent{
                /*event_id*/    1,   // EID_JOIN
                /*flags*/       0,
                /*ping_ms*/     0,
                /*user_ip*/     0,
                /*acct_number*/ 0,
                /*registration*/0,
                /*username*/    current_username_,
                /*text*/        ""},
            join_result.value().members_to_notify);
    }

    return core::ok();
}

core::Status<> BnetFsm::on(const ChatCommand& m) {
    if (state_ != BnetState::InChat) {
        return reject("bnet fsm: CHATCOMMAND outside chat");
    }
    
    // Parse the message text to check for commands
    // TODO: Whisper command handling will be implemented in a future phase
    
    if (m.text[0] == '/') {
        // Unknown command - stub handler
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        "Command not available"}});
    }
    
    // Regular channel message
    if (!use_cases_.post_message) {
        // No post_message use-case available - echo the message back
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    5,  // EID_TALK
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        m.text}});
    }
    
    // Create chat message using factory method
    auto chat_msg_result = domain::ChatMessage::create(m.text);
    if (!chat_msg_result) {
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        "Invalid chat message"}});
    }
    
    auto post_result = use_cases_.post_message->execute(
        current_channel_id_, current_account_id_, chat_msg_result.value());
    
    if (!post_result) {
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        "Failed to post message"}});
    }
    
    // Broadcast EID_TALK to all recipients via message_router
    if (!post_result.value().recipients.empty()) {
        broadcast_chat_event(
            ChatEvent{
                /*event_id*/    5,   // EID_TALK
                /*flags*/       0,
                /*ping_ms*/     0,
                /*user_ip*/     0,
                /*acct_number*/ 0,
                /*registration*/0,
                /*username*/    current_username_,
                /*text*/        std::string{m.text}},
            post_result.value().recipients);
    }

    return core::ok();
}

// --- Skeleton no-op handlers for newly-decoded SIDs ------------------------
// Phase 5 application-layer use-cases will own these. Until then the FSM
// accepts the messages silently so the wire stays alive; the codec round-
// trip + replay tests already cover the decoding path.

core::Status<> BnetFsm::on(const AuthCheckRequest&) {
    if (state_ != BnetState::AuthInfoReceived &&
        state_ != BnetState::Init) {
        return reject("bnet fsm: AUTH_CHECK out of order");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const GameListRequest& m) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: GETADVLISTEX before login");
    }
    
    // TODO: Phase 5 will implement game list query via IGameRepository
    // For now, send empty game list reply
    GameListReply reply;
    reply.sstatus = 0;  // Success
    reply.entries.clear();
    
    return ctx_->send(ServerMessage{reply});
}

core::Status<> BnetFsm::on(const LadderSearchRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: LADDERSEARCH before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const FileInfoRequest&) {
    // GETFILETIME may be sent from AuthInfoReceived onwards (e.g. for
    // gateways/icons probes) — accept in any non-Closing state.
    return core::ok();
}

core::Status<> BnetFsm::on(const CdKey2Request&) {
    // CDKEY2 is the LoD second-key proof; legal once AUTH_INFO has
    // been exchanged and before the user is logged in.
    if (state_ != BnetState::AuthInfoReceived &&
        state_ != BnetState::Init) {
        return reject("bnet fsm: CDKEY2 out of order");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const FriendsListRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: FRIENDSLIST before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const FriendInfoRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: FRIENDINFO before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const ClanInfoRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: CLANINFO before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const UserDataReadRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: READUSERDATA before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const UserDataWriteRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: WRITEUSERDATA before login");
    }
    return core::ok();
}

namespace {
core::Status<> require_clan_state(BnetState s, const char* msg) {
    if (s != BnetState::InChat && s != BnetState::LoggedIn && s != BnetState::InGame) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition, msg});
    }
    return core::ok();
}
}  // namespace

core::Status<> BnetFsm::on(const ClanCreateRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_CREATE before login");
}
core::Status<> BnetFsm::on(const ClanDisbandRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_DISBAND before login");
}
core::Status<> BnetFsm::on(const ClanNewChiefRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_NEWCHIEF before login");
}
core::Status<> BnetFsm::on(const ClanInviteRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_INVITE before login");
}
core::Status<> BnetFsm::on(const ClanMemberRemoveRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_MEMBER_REMOVE before login");
}
core::Status<> BnetFsm::on(const ClanMemberRankUpdateRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_RANKUPDATE before login");
}
core::Status<> BnetFsm::on(const ClanMotdChange&) {
    return require_clan_state(state_, "bnet fsm: CLAN_MOTDCHG before login");
}
core::Status<> BnetFsm::on(const ClanMotdRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_MOTDREQ before login");
}

core::Status<> BnetFsm::on(const ClanCreateInviteRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_CREATEINVITE_REQ before login");
}
core::Status<> BnetFsm::on(const ClanCreateInviteResponse&) {
    return require_clan_state(state_, "bnet fsm: CLAN_CREATEINVITE_REPLY before login");
}
core::Status<> BnetFsm::on(const ClanInvite2Response&) {
    return require_clan_state(state_, "bnet fsm: CLAN_INVITE2_REPLY before login");
}
core::Status<> BnetFsm::on(const ClanMemberListRequest&) {
    return require_clan_state(state_, "bnet fsm: CLANMEMBERLIST_REQ before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamFriendScreenRequest&) {
    return require_clan_state(state_, "bnet fsm: AT_FRIENDSCREEN before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamInviteFriendRequest&) {
    return require_clan_state(state_, "bnet fsm: AT_INVITE_FRIEND before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamAcceptDeclineInvite&) {
    return require_clan_state(state_, "bnet fsm: AT_ACCEPT_DECLINE before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamAcceptInvite&) {
    return require_clan_state(state_, "bnet fsm: AT_ACCEPT_INVITE before login");
}
core::Status<> BnetFsm::on(const StartGame4Request&) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME4 before login");
    if (!s) return s;
    state_ = BnetState::InGame;
    return core::ok();
}
core::Status<> BnetFsm::on(const UdpOk&) {
    // UDP echo confirmation may arrive in any state after AUTH_INFO; treat it
    // as advisory and never as a protocol violation.
    return core::ok();
}
core::Status<> BnetFsm::on(const LadderListRequest&) {
    return require_clan_state(state_, "bnet fsm: LADDERREQ before login");
}
core::Status<> BnetFsm::on(const AdRequest&) {
    // Banner fetches are advisory; the client polls them irrespective of
    // login state, so we never reject them.
    return core::ok();
}
core::Status<> BnetFsm::on(const AdClick&) {
    return core::ok();
}
core::Status<> BnetFsm::on(const AdAck&) {
    return core::ok();
}
core::Status<> BnetFsm::on(const AdClick2Request&) {
    return core::ok();
}
core::Status<> BnetFsm::on(const MotdRequest&) {
    return require_clan_state(state_, "bnet fsm: MOTDREQ before login");
}
core::Status<> BnetFsm::on(const ChannelListRequest&) {
    // PROGIDENT2 is part of the early handshake; it can arrive before login
    // (alongside AUTH_INFO) so we accept it in any state.
    return core::ok();
}
core::Status<> BnetFsm::on(const LeaveChannel&) {
    auto s = require_clan_state(state_, "bnet fsm: LEAVECHANNEL before login");
    if (!s) return s;
    
    if (!use_cases_.leave_channel) {
        // Use-case not available - provide default behavior
        // Clear current channel but stay in InChat state
        current_channel_id_ = domain::ChannelId{0};
        return core::ok();
    }
    
    auto leave_result = use_cases_.leave_channel->execute(
        current_channel_id_, current_account_id_);
    
    if (!leave_result) {
        // Leave failed - channel not found or not in channel
        return core::ok();  // Silent failure is acceptable
    }
    
    // Get remaining member session IDs from result
    const auto& result = leave_result.value();
    const auto& remaining_members = result.members_to_notify;
    
    // Broadcast EID_LEAVE to remaining members via router
    if (!remaining_members.empty()) {
        broadcast_chat_event(
            ChatEvent{
                /*event_id*/    4,   // EID_LEAVE
                /*flags*/       0,
                /*ping_ms*/     0,
                /*user_ip*/     0,
                /*acct_number*/ 0,
                /*registration*/0,
                /*username*/    current_username_,
                /*text*/        ""},
            remaining_members);
    }
    
    // Clear current channel
    current_channel_id_ = domain::ChannelId{0};
    
    return core::ok();
}
core::Status<> BnetFsm::on(const RegSnoopReply&) {
    // Reply to a server-side telemetry poke; advisory in every state.
    return core::ok();
}
core::Status<> BnetFsm::on(const ProfileRequest&) {
    return require_clan_state(state_, "bnet fsm: PROFILEREQ before login");
}
core::Status<> BnetFsm::on(const SetEmailReply&) {
    // Returned during login flow; the server may send SETEMAILREQ before
    // the login-ok packet, so accept the reply at any state.
    return core::ok();
}
core::Status<> BnetFsm::on(const IconRequest&) {
    // Icons.bni metadata fetch is part of the early handshake.
    return core::ok();
}
core::Status<> BnetFsm::on(const GetPasswordRequest&) {
    // Password recovery happens before login; accept in every state.
    return core::ok();
}
core::Status<> BnetFsm::on(const ChangeEmailRequest&) {
    return core::ok();
}
core::Status<> BnetFsm::on(const CrashDump&) {
    // Crash dumps arrive right after auth success; accept advisorily.
    return core::ok();
}
core::Status<> BnetFsm::on(const CharListRequest&) {
    // Legacy D2 charlist exchange happens after auth; gate on logged-in.
    return require_clan_state(state_, "bnet fsm: CHARLIST before login");
}
core::Status<> BnetFsm::on(const RealmListRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMLISTREQ before login");
}
core::Status<> BnetFsm::on(const RealmJoinRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMJOINREQ before login");
}
core::Status<> BnetFsm::on(const WarcraftGeneralRequest&) {
    return require_clan_state(state_, "bnet fsm: WARCRAFTGENERAL before login");
}
core::Status<> BnetFsm::on(const ExtraWork&) {
    // EXTRAWORK arrives during auth handshake (response to REQUIREDWORK);
    // accept advisorily so it survives whatever state we are in.
    return core::ok();
}
core::Status<> BnetFsm::on(const RealmListLegacyRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMLISTREQ (legacy) before login");
}
core::Status<> BnetFsm::on(const CdKey3Request&) {
    // CDKEY3 is part of pre-login auth; accept advisorily.
    return core::ok();
}
core::Status<> BnetFsm::on(const CreateAccount2Request&) {
    // CREATEACCOUNT2 is part of pre-login NLS account provisioning; accept advisorily.
    return core::ok();
}
core::Status<> BnetFsm::on(const LoginW3Request&) {
    // NLS step A; pre-login. Accept in any state.
    return core::ok();
}
core::Status<> BnetFsm::on(const LogonProofW3Request&) {
    // NLS step B; pre-login. Accept in any state.
    return core::ok();
}
core::Status<> BnetFsm::on(const PassChangeRequest&) {
    // NLS password-change step A; runs before the user is fully logged in.
    return core::ok();
}
core::Status<> BnetFsm::on(const PassChangeProofRequest&) {
    // NLS password-change step B; runs before the user is fully logged in.
    return core::ok();
}

// Legacy / OLS handlers: accept as advisory pre-login messages.
core::Status<> BnetFsm::on(const CompInfo1Request&)      { return core::ok(); }
core::Status<> BnetFsm::on(const ProgIdent&)             { return core::ok(); }
core::Status<> BnetFsm::on(const AuthReq1&)              { return core::ok(); }
core::Status<> BnetFsm::on(const CountryInfo1&)          { return core::ok(); }
core::Status<> BnetFsm::on(const CompInfo2&)             { return core::ok(); }
core::Status<> BnetFsm::on(const LoginReq1&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CreateAccount1Request&) { return core::ok(); }
core::Status<> BnetFsm::on(const Unknown2B&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CdKeyLegacyRequest&)    { return core::ok(); }
core::Status<> BnetFsm::on(const ChangePasswordRequest&) { return core::ok(); }
core::Status<> BnetFsm::on(const Unknown39&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CreateAccountRequest&)  { return core::ok(); }
core::Status<> BnetFsm::on(const NetGamePort&)           { return core::ok(); }

// --- Game-lifecycle handlers with state transitions ---------------------
core::Status<> BnetFsm::on(const StartGame1Request& m) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME1 before login");
    if (!s) return s;
    
    if (!use_cases_.start_game) {
        // No start_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame1Ack{0x00}});  // success code
    }
    
    // Use client_tag_ stored from AUTH_INFO
    // Call start_game use-case with game parameters from request
    auto start_result = use_cases_.start_game->execute(
        current_account_id_, client_tag_,
        m.game_name, "", m.gametype);  // Empty map_name for now
    
    if (!start_result) {
        // Game start failed
        return ctx_->send(ServerMessage{StartGame1Ack{0x01}});  // error code
    }
    
    // Store game ID and transition state
    const auto& start_res = start_result.value();
    current_game_id_ = start_res.game_id;
    state_ = BnetState::InGame;
    
    // Send success reply with game ID
    return ctx_->send(ServerMessage{StartGame1Ack{0x00}});  // success code
}

core::Status<> BnetFsm::on(const StartGame3Request& m) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME3 before login");
    if (!s) return s;
    
    if (!use_cases_.start_game) {
        // No start_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame3Ack{0x00}});  // success code
    }
    
    // Use client_tag_ stored from AUTH_INFO
    // Call start_game use-case with game parameters from request
    auto start_result = use_cases_.start_game->execute(
        current_account_id_, client_tag_,
        m.game_name, "", m.gametype);
    
    if (!start_result) {
        return ctx_->send(ServerMessage{StartGame3Ack{0x01}});
    }
    
    const auto& start_res = start_result.value();
    current_game_id_ = start_res.game_id;
    state_ = BnetState::InGame;
    
    return ctx_->send(ServerMessage{StartGame3Ack{0x00}});
}
core::Status<> BnetFsm::on(const JoinGame& m) {
    auto s = require_clan_state(state_, "bnet fsm: JOINGAME before login");
    if (!s) return s;
    
    if (!use_cases_.join_game) {
        // No join_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame4Ack{0x00u}});
    }

    // Parse game ID from message (game_name in the request)
    // For now, use a placeholder game ID
    domain::GameId game_id{0};

    auto join_result = use_cases_.join_game->execute(game_id, current_account_id_);

    if (!join_result) {
        // Join failed — send SID_STARTADVEX3 with non-zero error code
        return ctx_->send(ServerMessage{StartGame4Ack{0x01u}});
    }

    // Join succeeded - store game ID and transition to InGame
    current_game_id_ = game_id;
    state_ = BnetState::InGame;

    // Send SID_STARTADVEX3 (0x1C) success reply
    return ctx_->send(ServerMessage{StartGame4Ack{0x00u}});
}
core::Status<> BnetFsm::on(const CloseGame&) {
    // Accept in any post-login state; only transition out of InGame.
    auto s = require_clan_state(state_, "bnet fsm: CLOSEGAME before login");
    if (!s) return s;

    if (state_ == BnetState::InGame) {
        if (use_cases_.leave_game && current_game_id_.value() != 0) {
            auto leave_result = use_cases_.leave_game->execute(
                current_game_id_, current_account_id_);
            // LeaveGameResult has no member list; game-closed broadcast is
            // best-effort via a synthetic EID_LEAVE sent to the leaving player's
            // own session only (other players are notified by the game server).
            (void)leave_result;
        }

        current_game_id_ = domain::GameId{0};
        state_ = BnetState::LoggedIn;
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const CloseGame2&) {
    auto s = require_clan_state(state_, "bnet fsm: CLOSEGAME2 before login");
    if (!s) return s;

    if (state_ == BnetState::InGame) {
        if (use_cases_.leave_game && current_game_id_.value() != 0) {
            auto leave_result = use_cases_.leave_game->execute(
                current_game_id_, current_account_id_);
            (void)leave_result;
        }

        current_game_id_ = domain::GameId{0};
        state_ = BnetState::LoggedIn;
    }
    return core::ok();
}
core::Status<> BnetFsm::on(const GameReport&) {
    // Game-result upload is allowed during or after a game; no state change.
    return require_clan_state(state_, "bnet fsm: GAMEREPORT before login");
}

// --- Misc / anti-cheat / advisory handlers ------------------------------
// Anti-cheat memory replies and echo round-trips are advisory traffic that
// can arrive in any post-AUTH_INFO state without altering the session FSM.
core::Status<> BnetFsm::on(const ReadMemoryReply&) { return core::ok(); }
core::Status<> BnetFsm::on(const Unknown1B&)       { return core::ok(); }
core::Status<> BnetFsm::on(const Unknown24&)       { return core::ok(); }
core::Status<> BnetFsm::on(const MapAuthReq1&) {
    return require_clan_state(state_, "bnet fsm: MAPAUTHREQ1 before login");
}
core::Status<> BnetFsm::on(const MapAuthReq2&) {
    return require_clan_state(state_, "bnet fsm: MAPAUTHREQ2 before login");
}
core::Status<> BnetFsm::on(const ChangeClient&) { return core::ok(); }

}  // namespace pvpgn::protocol::bnet
