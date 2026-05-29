// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure codec for the Battle.net SID protocol.
///
/// `decode_client(packet)` / `decode_server(packet)` return a tagged
/// variant of all SIDs we currently understand. Unknown SIDs return
/// `core::Error{Unimplemented, "SID 0xNN"}` so the caller can choose to
/// log+drop or close. Malformed payloads return `OutOfRange` /
/// `InvalidArgument` from the underlying `Reader`.

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

/// Decode one inbound (client → server) SID packet.
core::Result<ClientMessage> decode_client(const Packet& pkt);

/// Decode one inbound (server → client) SID packet (used by replay
/// tests + d2cs/bnpcap-style tools).
core::Result<ServerMessage> decode_server(const Packet& pkt);

// --- encode -------------------------------------------------------------

core::Status<> encode(Writer& w, const Null&);
core::Status<> encode(Writer& w, const Ping&);
core::Status<> encode(Writer& w, const AuthInfo&);
core::Status<> encode(Writer& w, const AuthInfoReply&);
core::Status<> encode(Writer& w, const AuthCheckRequest&);
core::Status<> encode(Writer& w, const AuthCheckReply&);
core::Status<> encode(Writer& w, const LogonResponse2&);
core::Status<> encode(Writer& w, const LogonResponse2Reply&);
core::Status<> encode(Writer& w, const JoinChannel&);
core::Status<> encode(Writer& w, const EnterChatRequest&);
core::Status<> encode(Writer& w, const EnterChatReply&);
core::Status<> encode(Writer& w, const ChatCommand&);
core::Status<> encode(Writer& w, const ChatEvent&);
core::Status<> encode(Writer& w, const GameListRequest&);
core::Status<> encode(Writer& w, const GameListReply&);
core::Status<> encode(Writer& w, const LadderSearchRequest&);
core::Status<> encode(Writer& w, const LadderSearchReply&);
core::Status<> encode(Writer& w, const FileInfoRequest&);
core::Status<> encode(Writer& w, const FileInfoReply&);

core::Status<> encode(Writer& w, const CdKey2Request&);
core::Status<> encode(Writer& w, const CdKey2Reply&);
core::Status<> encode(Writer& w, const FriendsListRequest&);
core::Status<> encode(Writer& w, const FriendsListReply&);
core::Status<> encode(Writer& w, const FriendInfoRequest&);
core::Status<> encode(Writer& w, const FriendInfoReply&);
core::Status<> encode(Writer& w, const FriendAddAck&);
core::Status<> encode(Writer& w, const FriendDelAck&);
core::Status<> encode(Writer& w, const FriendMoveAck&);

core::Status<> encode(Writer& w, const ArrangedTeamFriendScreenRequest&);
core::Status<> encode(Writer& w, const ArrangedTeamFriendScreenReply&);
core::Status<> encode(Writer& w, const ArrangedTeamInviteFriendRequest&);
core::Status<> encode(Writer& w, const ArrangedTeamInviteFriendAck&);
core::Status<> encode(Writer& w, const ArrangedTeamMemberDecline&);
core::Status<> encode(Writer& w, const ArrangedTeamSendInvite&);
core::Status<> encode(Writer& w, const ArrangedTeamAcceptDeclineInvite&);
core::Status<> encode(Writer& w, const ArrangedTeamAcceptInvite&);

core::Status<> encode(Writer& w, const StartGame4Request&);
core::Status<> encode(Writer& w, const StartGame4Ack&);
core::Status<> encode(Writer& w, const UdpOk&);
core::Status<> encode(Writer& w, const LadderListRequest&);
core::Status<> encode(Writer& w, const LadderListReply&);
core::Status<> encode(Writer& w, const AdRequest&);
core::Status<> encode(Writer& w, const AdReply&);
core::Status<> encode(Writer& w, const AdClick&);
core::Status<> encode(Writer& w, const AdAck&);
core::Status<> encode(Writer& w, const AdClick2Request&);
core::Status<> encode(Writer& w, const AdClick2Reply&);
core::Status<> encode(Writer& w, const MotdRequest&);
core::Status<> encode(Writer& w, const MotdReply&);
core::Status<> encode(Writer& w, const ChannelListRequest&);
core::Status<> encode(Writer& w, const ChannelListReply&);
core::Status<> encode(Writer& w, const LeaveChannel&);
core::Status<> encode(Writer& w, const RegSnoopRequest&);
core::Status<> encode(Writer& w, const RegSnoopReply&);
core::Status<> encode(Writer& w, const ProfileRequest&);
core::Status<> encode(Writer& w, const ProfileReply&);
core::Status<> encode(Writer& w, const SetEmailRequest&);
core::Status<> encode(Writer& w, const SetEmailReply&);
core::Status<> encode(Writer& w, const IconRequest&);
core::Status<> encode(Writer& w, const IconReply&);
core::Status<> encode(Writer& w, const GetPasswordRequest&);
core::Status<> encode(Writer& w, const ChangeEmailRequest&);
core::Status<> encode(Writer& w, const CrashDump&);
core::Status<> encode(Writer& w, const CharListRequest&);
core::Status<> encode(Writer& w, const CharListReply&);
core::Status<> encode(Writer& w, const ServerList&);
core::Status<> encode(Writer& w, const MessageBox&);
core::Status<> encode(Writer& w, const RealmListRequest&);
core::Status<> encode(Writer& w, const RealmListReply&);
core::Status<> encode(Writer& w, const RealmJoinRequest&);
core::Status<> encode(Writer& w, const RealmJoinReply&);
core::Status<> encode(Writer& w, const WarcraftGeneralRequest&);
core::Status<> encode(Writer& w, const WarcraftGeneralReply&);
core::Status<> encode(Writer& w, const RequiredWork&);
core::Status<> encode(Writer& w, const ExtraWork&);
core::Status<> encode(Writer& w, const RealmListLegacyRequest&);
core::Status<> encode(Writer& w, const RealmListLegacyReply&);
core::Status<> encode(Writer& w, const CdKey3Request&);
core::Status<> encode(Writer& w, const CdKey3Reply&);
core::Status<> encode(Writer& w, const CreateAccount2Request&);
core::Status<> encode(Writer& w, const CreateAccount2Reply&);
core::Status<> encode(Writer& w, const LoginW3Request&);
core::Status<> encode(Writer& w, const LoginW3Reply&);
core::Status<> encode(Writer& w, const LogonProofW3Request&);
core::Status<> encode(Writer& w, const LogonProofW3Reply&);
core::Status<> encode(Writer& w, const PassChangeRequest&);
core::Status<> encode(Writer& w, const PassChangeReply&);
core::Status<> encode(Writer& w, const PassChangeProofRequest&);
core::Status<> encode(Writer& w, const PassChangeProofReply&);

// Legacy / OLS encoders.
core::Status<> encode(Writer& w, const CompInfo1Request&);
core::Status<> encode(Writer& w, const CompReply&);
core::Status<> encode(Writer& w, const ProgIdent&);
core::Status<> encode(Writer& w, const AuthReq1Server&);
core::Status<> encode(Writer& w, const AuthReq1&);
core::Status<> encode(Writer& w, const AuthReply1&);
core::Status<> encode(Writer& w, const CountryInfo1&);
core::Status<> encode(Writer& w, const SessionKey1&);
core::Status<> encode(Writer& w, const SessionKey2&);
core::Status<> encode(Writer& w, const CompInfo2&);
core::Status<> encode(Writer& w, const LoginReq1&);
core::Status<> encode(Writer& w, const LoginReply1&);
core::Status<> encode(Writer& w, const CreateAccount1Request&);
core::Status<> encode(Writer& w, const CreateAccount1Reply&);
core::Status<> encode(Writer& w, const Unknown2B&);
core::Status<> encode(Writer& w, const CdKeyLegacyRequest&);
core::Status<> encode(Writer& w, const CdKeyLegacyReply&);
core::Status<> encode(Writer& w, const ChangePasswordRequest&);
core::Status<> encode(Writer& w, const ChangePasswordReply&);
core::Status<> encode(Writer& w, const Unknown39&);
core::Status<> encode(Writer& w, const CreateAccountRequest&);
core::Status<> encode(Writer& w, const CreateAccountReply&);
core::Status<> encode(Writer& w, const NetGamePort&);
// Game-lifecycle SIDs.
core::Status<> encode(Writer& w, const CloseGame&);
core::Status<> encode(Writer& w, const CloseGame2&);
core::Status<> encode(Writer& w, const StartGame1Request&);
core::Status<> encode(Writer& w, const StartGame1Ack&);
core::Status<> encode(Writer& w, const StartGame3Request&);
core::Status<> encode(Writer& w, const StartGame3Ack&);
core::Status<> encode(Writer& w, const JoinGame&);
core::Status<> encode(Writer& w, const GameReport&);
// Misc / anti-cheat / advisory.
core::Status<> encode(Writer& w, const ReadMemoryRequest&);
core::Status<> encode(Writer& w, const ReadMemoryReply&);
core::Status<> encode(Writer& w, const Unknown1B&);
core::Status<> encode(Writer& w, const Unknown24&);
core::Status<> encode(Writer& w, const MapAuthReq1&);
core::Status<> encode(Writer& w, const MapAuthReply1&);
core::Status<> encode(Writer& w, const MapAuthReq2&);
core::Status<> encode(Writer& w, const MapAuthReply2&);
core::Status<> encode(Writer& w, const ChangeClient&);
core::Status<> encode(Writer& w, const ClanInfoRequest&);
core::Status<> encode(Writer& w, const ClanInfoReply&);

core::Status<> encode(Writer& w, const UserDataReadRequest&);
core::Status<> encode(Writer& w, const UserDataReadReply&);
core::Status<> encode(Writer& w, const UserDataWriteRequest&);

core::Status<> encode(Writer& w, const ClanCreateRequest&);
core::Status<> encode(Writer& w, const ClanCreateReply&);
core::Status<> encode(Writer& w, const ClanDisbandRequest&);
core::Status<> encode(Writer& w, const ClanNewChiefRequest&);
core::Status<> encode(Writer& w, const ClanInviteRequest&);
core::Status<> encode(Writer& w, const ClanMemberRemoveRequest&);
core::Status<> encode(Writer& w, const ClanMemberRankUpdateRequest&);
core::Status<> encode(Writer& w, const ClanGenericResultReply&);
core::Status<> encode(Writer& w, const ClanMotdChange&);
core::Status<> encode(Writer& w, const ClanMotdRequest&);
core::Status<> encode(Writer& w, const ClanMotdReply&);

core::Status<> encode(Writer& w, const ClanCreateInviteRequest&);
core::Status<> encode(Writer& w, const ClanCreateInviteSummary&);
core::Status<> encode(Writer& w, const ClanCreateInviteForward&);
core::Status<> encode(Writer& w, const ClanCreateInviteResponse&);
core::Status<> encode(Writer& w, const ClanInvite2Forward&);
core::Status<> encode(Writer& w, const ClanInvite2Response&);

core::Status<> encode(Writer& w, const ClanMemberListRequest&);
core::Status<> encode(Writer& w, const ClanMemberListReply&);
core::Status<> encode(Writer& w, const ClanMemberRemovedNotify&);
core::Status<> encode(Writer& w, const ClanMemberUpdate&);

}  // namespace pvpgn::protocol::bnet
