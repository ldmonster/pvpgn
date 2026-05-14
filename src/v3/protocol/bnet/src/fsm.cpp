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

core::Status<> BnetFsm::on(const GameListRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: GETADVLISTEX before login");
    }
    return core::ok();
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
    return require_clan_state(state_, "bnet fsm: LEAVECHANNEL before login");
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
core::Status<> BnetFsm::on(const StartGame1Request&) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME1 before login");
    if (!s) return s;
    state_ = BnetState::InGame;
    return core::ok();
}
core::Status<> BnetFsm::on(const StartGame3Request&) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME3 before login");
    if (!s) return s;
    state_ = BnetState::InGame;
    return core::ok();
}
core::Status<> BnetFsm::on(const JoinGame&) {
    auto s = require_clan_state(state_, "bnet fsm: JOINGAME before login");
    if (!s) return s;
    state_ = BnetState::InGame;
    return core::ok();
}
core::Status<> BnetFsm::on(const CloseGame&) {
    // Accept in any post-login state; only transition out of InGame.
    auto s = require_clan_state(state_, "bnet fsm: CLOSEGAME before login");
    if (!s) return s;
    if (state_ == BnetState::InGame) state_ = BnetState::LoggedIn;
    return core::ok();
}
core::Status<> BnetFsm::on(const CloseGame2&) {
    auto s = require_clan_state(state_, "bnet fsm: CLOSEGAME2 before login");
    if (!s) return s;
    if (state_ == BnetState::InGame) state_ = BnetState::LoggedIn;
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
