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
///     are delegated to `ISessionContext`; for now the FSM
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context.hpp"
#include "protocol/bnet/use_case_context.hpp"

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
    /// Construct the FSM.
    /// @param ctx         Session I/O context (send / close).
    /// @param use_cases   Injected application use-cases (may be null).
    /// @param session_id  This session's identity, used when broadcasting
    ///                    via message_router so the sender is excluded.
    explicit BnetFsm(std::shared_ptr<ISessionContext> ctx,
                     const BnetUseCaseContext& use_cases,
                     domain::SessionId session_id = domain::SessionId{}) noexcept
        : ctx_(ctx), use_cases_(use_cases), session_id_(session_id) {}

    BnetState state() const noexcept { return state_; }

    /// Drive the FSM with one decoded inbound message. Returns
    /// `InvalidArgument` if the message is illegal in the current
    /// state (the caller should close the session).
    core::Status<> handle(const ClientMessage& msg);

    /// Called by the transport when the connection closes. If the client is
    /// still in a channel, notify the remaining members with EID_LEAVE (the
    /// original broadcasts a channel part on disconnect, not just on an
    /// explicit SID_LEAVECHANNEL).
    void on_disconnect();

    // Visitor handlers — public so a custom dispatcher can call them.
    core::Status<> on(const Null&);
    core::Status<> on(const Ping&);
    core::Status<> on(const AuthInfo&);
    core::Status<> on(const LogonResponse2&);
    core::Status<> on(const JoinChannel&);
    core::Status<> on(const EnterChatRequest&);
    core::Status<> on(const ChatCommand&);

    // Newer SIDs decoded but not yet driven by the FSM skeleton.
    // The application-layer use-cases will own these. For now
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

    /// Encode a ChatEvent and broadcast it to `sessions` via message_router.
    /// No-op if message_router is null or sessions is empty.
    /// Ignores send errors (best-effort broadcast).
    void broadcast_chat_event(const ChatEvent& ev,
                              std::span<const domain::SessionId> sessions);

    /// Handle the /whisper command family (/w /msg /m). `rest` is the command
    /// line past the leading '/', `cmd_end` the offset of the space after the
    /// command word (npos if none). Routes EID_WHISPER to the target session
    /// and EID_WHISPERSENT back to the sender.
    core::Status<> handle_whisper(std::string_view rest,
                                  std::size_t cmd_end);

    /// Handle the /me (and /emote) command: broadcast EID_EMOTE with `body` to
    /// the other members of the current channel. Errors to the sender when not
    /// in a channel or the body is empty.
    core::Status<> handle_emote(std::string_view body);

    std::shared_ptr<ISessionContext> ctx_;
    BnetUseCaseContext use_cases_;
    BnetState state_ = BnetState::Init;

    // Per-session identity
    domain::SessionId session_id_{};

    // Session tracking
    domain::AccountId current_account_id_{0};
    domain::ChannelId current_channel_id_{0};
    domain::GameId    current_game_id_{0};

    /// Client product tag stored from AUTH_INFO (e.g. STAR, D2DV, WAR3).
    /// Default-constructed (all-zero) until AUTH_INFO is received.
    domain::ClientTag client_tag_{};

    /// Server token issued in the SID_AUTH_INFO reply (0x50). The client folds
    /// it into the OLS password double-hash and echoes it back in
    /// SID_LOGONRESPONSE2. 0 until AUTH_INFO is processed.
    std::uint32_t server_token_ = 0;

    /// Client version id taken verbatim from SID_AUTH_INFO (0x50). The original
    /// gates its "no e-mail on file" prompt on `versionid >= 0x0D` after a
    /// successful W3 NLS proof (handle_bnet.cpp _client_loginproofw3); we mirror
    /// that gate. 0 until AUTH_INFO is processed.
    std::uint32_t version_id_ = 0;

    /// Username stored at login time, used in broadcast ChatEvents.
    std::string current_username_;

    // --- WarCraft III SRP-3 challenge state -----------------------------
    // Held between SID_AUTH_ACCOUNTLOGON (0x53) and ..._PROOF (0x54). The
    // challenge step pre-computes the expected client proof M1 and the server
    // proof M2 (mirroring the original); the proof step is a 20-byte compare.
    bool                         w3_challenge_ready_ = false;
    std::array<std::uint8_t, 20> w3_expected_m1_{};
    std::array<std::uint8_t, 20> w3_server_m2_{};
    domain::AccountId            w3_pending_account_{0};
    std::string                  w3_pending_username_;
};

}  // namespace pvpgn::protocol::bnet
