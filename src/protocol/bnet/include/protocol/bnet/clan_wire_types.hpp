// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_wire_types.hpp
/// Battle.net clan-management wire constants (CLIENT_CLAN_*,
/// SERVER_CLAN_*, ARRANGEDTEAM_ACCEPT_INVITE, CLAN_RESPONSE_*) and
/// CLAN-INFO request/reply, mirrored from `src/common/bnet_protocol.h`
/// (lines ~3686-4068). Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::clan {

// ---- Packet type codes -------------------------------------------------
inline constexpr std::uint16_t kClientArrangedTeamAcceptInvite = 0xfdff;

inline constexpr std::uint16_t kClientClanInfoReq         = 0x82ff;
inline constexpr std::uint16_t kServerClanInfoReply       = 0x82ff;

inline constexpr std::uint16_t kClientClanCreateReq       = 0x70ff;
inline constexpr std::uint16_t kServerClanCreateReply     = 0x70ff;
inline constexpr std::uint16_t kClientClanCreateInviteReq = 0x71ff;
inline constexpr std::uint16_t kServerClanCreateInviteReply = 0x71ff;
inline constexpr std::uint16_t kServerClanCreateInviteReq = 0x72ff;
inline constexpr std::uint16_t kClientClanCreateInviteReply = 0x72ff;
inline constexpr std::uint16_t kClientClanDisbandReq      = 0x73ff;
inline constexpr std::uint16_t kServerClanDisbandReply    = 0x73ff;
inline constexpr std::uint16_t kClientClanMemberNewChiefReq   = 0x74ff;
inline constexpr std::uint16_t kServerClanMemberNewChiefReply = 0x74ff;
inline constexpr std::uint16_t kServerClanClanAck         = 0x75ff;
inline constexpr std::uint16_t kServerClanQuitNotify      = 0x76ff;
inline constexpr std::uint16_t kClientClanInviteReq       = 0x77ff;
inline constexpr std::uint16_t kServerClanInviteReply     = 0x77ff;
inline constexpr std::uint16_t kClientClanMemberRemoveReq   = 0x78ff;
inline constexpr std::uint16_t kServerClanMemberRemoveReply = 0x78ff;
inline constexpr std::uint16_t kServerClanInviteReq       = 0x79ff;
inline constexpr std::uint16_t kClientClanInviteReply     = 0x79ff;
inline constexpr std::uint16_t kClientClanMemberRankUpdateReq   = 0x7aff;
inline constexpr std::uint16_t kServerClanMemberRankUpdateReply = 0x7aff;
inline constexpr std::uint16_t kClientClanMotdChg         = 0x7bff;
inline constexpr std::uint16_t kServerClanMotdReply       = 0x7cff;
inline constexpr std::uint16_t kClientClanMotdReq         = 0x7cff;
inline constexpr std::uint16_t kServerClanMemberListReply = 0x7dff;
inline constexpr std::uint16_t kClientClanMemberListReq   = 0x7dff;
inline constexpr std::uint16_t kServerClanMemberRemovedNotify = 0x7eff;
inline constexpr std::uint16_t kServerClanMemberUpdate    = 0x7fff;

// ---- Clan member rank (1-byte) ----------------------------------------
inline constexpr std::uint8_t kRankNew       = 0x00;
inline constexpr std::uint8_t kRankPeon      = 0x01;
inline constexpr std::uint8_t kRankGrunt     = 0x02;
inline constexpr std::uint8_t kRankShaman    = 0x03;
inline constexpr std::uint8_t kRankChieftain = 0x04;

// ---- Clan member presence (1-byte) -- note: kPresenceOffline aliases ---
//      the same byte value as kRankNew in the legacy header. ----------
inline constexpr std::uint8_t kPresenceOffline     = 0x00;
inline constexpr std::uint8_t kPresenceOnline      = 0x01;
inline constexpr std::uint8_t kPresenceChannel     = 0x02;
inline constexpr std::uint8_t kPresenceGame        = 0x03;
inline constexpr std::uint8_t kPresencePrivateGame = 0x04;

// ---- ClanCreateReply check_result -------------------------------------
inline constexpr std::uint8_t kCreateReplyCheckOk             = 0x00;
inline constexpr std::uint8_t kCreateReplyCheckAlreadyInUse   = 0x01;
inline constexpr std::uint8_t kCreateReplyCheckTimeLimit      = 0x02;
inline constexpr std::uint8_t kCreateReplyCheckException      = 0x04;
inline constexpr std::uint8_t kCreateReplyCheckInvalidClanTag = 0x0a;

// ---- ClanDisbandReply result ------------------------------------------
inline constexpr std::uint8_t kDisbandReplyResultOk        = 0x0;
inline constexpr std::uint8_t kDisbandReplyResultException = 0x1;
inline constexpr std::uint8_t kDisbandReplyResultFailed    = 0x2;

// ---- ClanMemberNewChiefReply result -----------------------------------
inline constexpr std::uint8_t kMemberNewChiefSuccess = 0x00;
inline constexpr std::uint8_t kMemberNewChiefFailed  = 0x01;

// ---- ClanQuitNotify status --------------------------------------------
inline constexpr std::uint8_t kQuitNotifyStatusRemovedFromClan = 0x01;

// ---- ClanMemberRemoveReply result -------------------------------------
inline constexpr std::uint8_t kMemberRemoveSuccess = 0x00;
inline constexpr std::uint8_t kMemberRemoveFailed  = 0x01;

// ---- ClanMemberRankUpdateReply result ---------------------------------
inline constexpr std::uint8_t kMemberRankUpdateSuccess = 0x00;
inline constexpr std::uint8_t kMemberRankUpdateFailed  = 0x01;

// ---- ClanMotdReply unknow1 placeholder --------------------------------
inline constexpr std::uint32_t kMotdReplyUnknow1 = 0x00000000;

// ---- CLAN_RESPONSE_* shared response codes (bnetdocs) -----------------
inline constexpr std::uint8_t kResponseSuccess       = 0x00;
inline constexpr std::uint8_t kResponseFail          = 0x01;
inline constexpr std::uint8_t kResponseTooSoon       = 0x02;
inline constexpr std::uint8_t kResponseTooSmall      = 0x03;
inline constexpr std::uint8_t kResponseDeclined      = 0x04;
inline constexpr std::uint8_t kResponseDecline       = 0x05;
inline constexpr std::uint8_t kResponseAccept        = 0x06;
inline constexpr std::uint8_t kResponseNotAuthorized = 0x07;
inline constexpr std::uint8_t kResponseNotFound      = 0x08;
inline constexpr std::uint8_t kResponseClanFull      = 0x09;
inline constexpr std::uint8_t kResponseBadTag        = 0x0a;
inline constexpr std::uint8_t kResponseBadName       = 0x0b;
inline constexpr std::uint8_t kResponseNotMember     = 0x0c;

} // namespace pvpgn::protocol::bnet::clan
