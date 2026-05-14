// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fsm.hpp
/// Per-session Battle.net protocol FSM.
///
/// **What this class does**
///   * Holds the wire-level state (`BnetState`) so the codec stays pure.
///   * Validates that an incoming message is legal in the current state
///     (rejects out-of-order traffic before any use-case fires).
///   * Sends the canonical immediate replies that the protocol demands
///     (PING echo, LOGON acks, ENTERCHAT echo). Real domain mutations
///     are delegated to `ISessionContext` in Phase 5; for now the FSM
///     only performs the wire dance and records what it saw.
///
/// **What this class does NOT do**
///   * No I/O — `send()` goes through `ISessionContext`.
///   * No clock — `Ping` reflects the client cookie verbatim.
///   * No domain access — no `AccountId`, no `Channel` here.
///
/// State chart:
///
///       ┌──────────┐   AUTH_INFO   ┌────────────────────┐
///       │   Init   │──────────────▶│  AuthInfoReceived  │
///       └──────────┘               └─────────┬──────────┘
///                                            │ LOGONRESPONSE2 (ok)
///                                            ▼
///                                  ┌────────────────────┐
///                                  │     LoggedIn       │
///                                  └─────────┬──────────┘
///                                            │ ENTERCHAT
///                                            ▼
///                                  ┌────────────────────┐
///                                  │      InChat        │
///                                  └────────────────────┘
///
/// `Ping` and `Null` are legal in every non-`Closing` state.

#include <cstdint>

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context.hpp"

namespace pvpgn::protocol::bnet {

enum class BnetState : std::uint8_t {
    Init,
    AuthInfoReceived,
    LoggedIn,
    InChat,
    InGame,
    Closing,
};

class BnetFsm {
public:
    explicit BnetFsm(ISessionContext& ctx) noexcept : ctx_(&ctx) {}

    BnetState state() const noexcept { return state_; }

    /// Drive the FSM with one decoded inbound message. Returns
    /// `InvalidArgument` if the message is illegal in the current
    /// state (the caller should close the session).
    core::Status<> handle(const ClientMessage& msg);

    // Visitor handlers — public so a custom dispatcher can call them.
    core::Status<> on(const Null&);
    core::Status<> on(const Ping&);
    core::Status<> on(const AuthInfo&);
    core::Status<> on(const LogonResponse2&);
    core::Status<> on(const JoinChannel&);
    core::Status<> on(const EnterChatRequest&);
    core::Status<> on(const ChatCommand&);

    // Newer SIDs decoded but not yet driven by the FSM skeleton.
    // The Phase 5 application-layer use-cases will own these. For now
    // the FSM accepts them as no-ops so unknown protocol traffic in
    // a valid state doesn't tear down the session.
    core::Status<> on(const AuthCheckRequest&);
    core::Status<> on(const GameListRequest&);
    core::Status<> on(const LadderSearchRequest&);
    core::Status<> on(const FileInfoRequest&);
    core::Status<> on(const CdKey2Request&);
    core::Status<> on(const FriendsListRequest&);
    core::Status<> on(const FriendInfoRequest&);
    core::Status<> on(const ClanInfoRequest&);
    core::Status<> on(const UserDataReadRequest&);
    core::Status<> on(const UserDataWriteRequest&);
    core::Status<> on(const ClanCreateRequest&);
    core::Status<> on(const ClanDisbandRequest&);
    core::Status<> on(const ClanNewChiefRequest&);
    core::Status<> on(const ClanInviteRequest&);
    core::Status<> on(const ClanMemberRemoveRequest&);
    core::Status<> on(const ClanMemberRankUpdateRequest&);
    core::Status<> on(const ClanMotdChange&);
    core::Status<> on(const ClanMotdRequest&);
    core::Status<> on(const ClanCreateInviteRequest&);
    core::Status<> on(const ClanCreateInviteResponse&);
    core::Status<> on(const ClanInvite2Response&);
    core::Status<> on(const ClanMemberListRequest&);
    core::Status<> on(const ArrangedTeamFriendScreenRequest&);
    core::Status<> on(const ArrangedTeamInviteFriendRequest&);
    core::Status<> on(const ArrangedTeamAcceptDeclineInvite&);
    core::Status<> on(const ArrangedTeamAcceptInvite&);
    core::Status<> on(const StartGame4Request&);
    core::Status<> on(const UdpOk&);
    core::Status<> on(const LadderListRequest&);
    core::Status<> on(const AdRequest&);
    core::Status<> on(const AdClick&);
    core::Status<> on(const AdAck&);
    core::Status<> on(const AdClick2Request&);
    core::Status<> on(const MotdRequest&);
    core::Status<> on(const ChannelListRequest&);
    core::Status<> on(const LeaveChannel&);
    core::Status<> on(const RegSnoopReply&);
    core::Status<> on(const ProfileRequest&);
    core::Status<> on(const SetEmailReply&);
    core::Status<> on(const IconRequest&);
    core::Status<> on(const GetPasswordRequest&);
    core::Status<> on(const ChangeEmailRequest&);
    core::Status<> on(const CrashDump&);
    core::Status<> on(const CharListRequest&);
    core::Status<> on(const RealmListRequest&);
    core::Status<> on(const RealmJoinRequest&);
    core::Status<> on(const WarcraftGeneralRequest&);
    core::Status<> on(const ExtraWork&);
    core::Status<> on(const RealmListLegacyRequest&);
    core::Status<> on(const CdKey3Request&);
    core::Status<> on(const CreateAccount2Request&);
    core::Status<> on(const LoginW3Request&);
    core::Status<> on(const LogonProofW3Request&);
    core::Status<> on(const PassChangeRequest&);
    core::Status<> on(const PassChangeProofRequest&);

    // Legacy / OLS handlers.
    core::Status<> on(const CompInfo1Request&);
    core::Status<> on(const ProgIdent&);
    core::Status<> on(const AuthReq1&);
    core::Status<> on(const CountryInfo1&);
    core::Status<> on(const CompInfo2&);
    core::Status<> on(const LoginReq1&);
    core::Status<> on(const CreateAccount1Request&);
    core::Status<> on(const Unknown2B&);
    core::Status<> on(const CdKeyLegacyRequest&);
    core::Status<> on(const ChangePasswordRequest&);
    core::Status<> on(const Unknown39&);
    core::Status<> on(const CreateAccountRequest&);
    core::Status<> on(const NetGamePort&);

    // Game-lifecycle handlers with FSM state transitions.
    core::Status<> on(const CloseGame&);
    core::Status<> on(const CloseGame2&);
    core::Status<> on(const StartGame1Request&);
    core::Status<> on(const StartGame3Request&);
    core::Status<> on(const JoinGame&);
    core::Status<> on(const GameReport&);

    // Misc / anti-cheat / advisory.
    core::Status<> on(const ReadMemoryReply&);
    core::Status<> on(const Unknown1B&);
    core::Status<> on(const Unknown24&);
    core::Status<> on(const MapAuthReq1&);
    core::Status<> on(const MapAuthReq2&);
    core::Status<> on(const ChangeClient&);

private:
    core::Status<> reject(const char* reason);

    ISessionContext* ctx_;
    BnetState        state_ = BnetState::Init;
};

}  // namespace pvpgn::protocol::bnet
