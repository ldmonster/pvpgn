// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file anongame_wire_types.hpp
/// Anongame (matchmaking + arranged-team + friends) wire constants,
/// mirrored from `src/common/anongame_protocol.h`.
///
/// The legacy structs embed `t_bnet_header` from the as-yet-unported
/// `src/common/bnet_protocol.h` (3.6 kLOC), so per-message struct
/// shapes are deliberately deferred until that header lands. This
/// file pins the wire-visible numeric constants — packet type codes,
/// option discriminators, friend status / type bytes, ANONGAME_TYPE_*
/// enum, and the ASCII "tag" strings — to their legacy values so
/// codec parity can be unit-tested today.

#include <cstdint>

namespace pvpgn::protocol::bnet::anongame {

// ---- 16-bit packet type codes (bnet protocol class) -------------------

inline constexpr std::uint16_t kClientFindAnongame                = 0x44ff;
inline constexpr std::uint16_t kServerFindAnongame                = 0x44ff;
inline constexpr std::uint16_t kServerAnongameSearchReply         = 0x44ff;
inline constexpr std::uint16_t kServerAnongameFound               = 0x44ff;
inline constexpr std::uint16_t kClientFindAnongameInfoReq         = 0x44ff;
inline constexpr std::uint16_t kServerFindAnongameInfoReply       = 0x44ff;
inline constexpr std::uint16_t kServerFindAnongamePlayGameCancel  = 0x44ff;
inline constexpr std::uint16_t kServerFindAnongameProfile         = 0x44ff;
inline constexpr std::uint16_t kClientFindAnongameTournamentReq   = 0x44ff;
inline constexpr std::uint16_t kServerFindAnongameTournamentReply = 0x44ff;
inline constexpr std::uint16_t kServerFindAnongameProfileClan     = 0x44ff;
inline constexpr std::uint16_t kServerFindAnongameIconReply       = 0x44ff;

inline constexpr std::uint16_t kClientArrangedTeamFriendScreen    = 0x60ff;
inline constexpr std::uint16_t kServerArrangedTeamFriendScreen    = 0x60ff;
inline constexpr std::uint16_t kClientArrangedTeamInviteFriend    = 0x61ff;
inline constexpr std::uint16_t kServerArrangedTeamInviteFriendAck = 0x61ff;
inline constexpr std::uint16_t kServerArrangedTeamMemberDecline   = 0x62ff;
inline constexpr std::uint16_t kServerArrangedTeamSendInvite      = 0x63ff;
inline constexpr std::uint16_t kClientArrangedTeamAcceptDecline   = 0x63ff;

inline constexpr std::uint16_t kClientFriendsListReq              = 0x65ff;
inline constexpr std::uint16_t kServerFriendsListReply            = 0x65ff;
inline constexpr std::uint16_t kClientFriendInfoReq               = 0x66ff;
inline constexpr std::uint16_t kServerFriendInfoReply             = 0x66ff;
inline constexpr std::uint16_t kServerFriendAddAck                = 0x67ff;
inline constexpr std::uint16_t kServerFriendDelAck                = 0x68ff;
inline constexpr std::uint16_t kServerFriendMoveAck               = 0x69ff;

// ---- Client findanongame option byte (first byte after header) -------

inline constexpr std::uint8_t kClientFindAnongameSearch          = 0x00;
inline constexpr std::uint8_t kClientFindAnongameInfos           = 0x02;
inline constexpr std::uint8_t kClientFindAnongameCancel          = 0x03;
inline constexpr std::uint8_t kClientFindAnongameProfile         = 0x04;
inline constexpr std::uint8_t kClientFindAnongameAtSearch        = 0x05;
inline constexpr std::uint8_t kClientFindAnongameAtInviterSearch = 0x06;
inline constexpr std::uint8_t kClientAnongameTournament          = 0x07;
inline constexpr std::uint8_t kClientFindAnongameProfileClan     = 0x08;
inline constexpr std::uint8_t kClientFindAnongameGetIcon         = 0x09;
inline constexpr std::uint8_t kClientFindAnongameSetIcon         = 0x0A;

// ---- Server findanongame option byte ---------------------------------

inline constexpr std::uint8_t kServerFindAnongameSearch  = 0x00;
inline constexpr std::uint8_t kServerFindAnongameFound   = 0x01;
inline constexpr std::uint8_t kServerFindAnongameCancel  = 0x03;

// ---- Anongame info-tag strings (4-byte ASCII, big-endian as int) -----

inline constexpr std::uint32_t kClientFindAnongameInfoTagUrl  = 0x55524cu;     // "URL\0"
inline constexpr std::uint32_t kClientFindAnongameInfoTagMap  = 0x4d4150u;     // "MAP\0"
inline constexpr std::uint32_t kClientFindAnongameInfoTagType = 0x54595045u;   // "TYPE"
inline constexpr std::uint32_t kClientFindAnongameInfoTagDesc = 0x44455343u;   // "DESC"
inline constexpr std::uint32_t kClientFindAnongameInfoTagLadr = 0x4c414452u;   // "LADR"
inline constexpr std::uint32_t kClientFindAnongameInfoTagSolo = 0x534f4c4fu;   // "SOLO"
inline constexpr std::uint32_t kClientFindAnongameInfoTagTeam = 0x5445414du;   // "TEAM"
inline constexpr std::uint32_t kClientFindAnongameInfoTagFfa  = 0x46464120u;   // "FFA "

// Server "anongame_string" tag values
inline constexpr std::uint32_t kServerAnongameSoloStr = 0x534F4C4Fu;  // "SOLO"
inline constexpr std::uint32_t kServerAnongameTeamStr = 0x5445414Du;  // "TEAM"
inline constexpr std::uint32_t kServerAnongameSffaStr = 0x46464120u;  // "FFA "
inline constexpr std::uint32_t kServerAnongameAt2v2Str = 0x32565332u; // "2VS2"
inline constexpr std::uint32_t kServerAnongameAt3v3Str = 0x33565333u; // "3VS3"
inline constexpr std::uint32_t kServerAnongameAt4v4Str = 0x34565334u; // "4VS4"
inline constexpr std::uint32_t kServerAnongameTyStr    = 0x54592020u; // "TY  "

inline constexpr std::uint32_t kServerFindAnongameProfileUnknown2 = 0x6E736865u;  // "nshe" ?

// ---- ANONGAME_TYPE_* ladder/queue codes ------------------------------

inline constexpr int kAnongameType1v1       = 0;
inline constexpr int kAnongameType2v2       = 1;
inline constexpr int kAnongameType3v3       = 2;
inline constexpr int kAnongameType4v4       = 3;
inline constexpr int kAnongameTypeSmallFfa  = 4;
inline constexpr int kAnongameTypeAt2v2     = 5;
inline constexpr int kAnongameTypeTeamFfa   = 6;
inline constexpr int kAnongameTypeAt3v3     = 7;
inline constexpr int kAnongameTypeAt4v4     = 8;
inline constexpr int kAnongameTypeTy        = 9;
inline constexpr int kAnongameType5v5       = 10;
inline constexpr int kAnongameType6v6       = 11;
inline constexpr int kAnongameType2v2v2     = 12;
inline constexpr int kAnongameType3v3v3     = 13;
inline constexpr int kAnongameType4v4v4     = 14;
inline constexpr int kAnongameType2v2v2v2   = 15;
inline constexpr int kAnongameType3v3v3v3   = 16;
inline constexpr int kAnongameTypeAt2v2v2   = 17;
inline constexpr int kAnongameTypes         = 18;  // count

// ---- Arranged-team accept/decline action codes (uint32 in packet) ----

inline constexpr std::uint32_t kClientArrangedTeamAccept  = 0x00000003;
inline constexpr std::uint32_t kClientArrangedTeamDecline = 0x00000002;
inline constexpr std::uint32_t kServerArrangedTeamAccept  = 0x00000003;
inline constexpr std::uint32_t kServerArrangedTeamDecline = 0x00000002;

inline constexpr std::uint8_t kServerArrangedTeamAddName = 0x01;

// ---- Friend type / status bytes --------------------------------------

inline constexpr std::uint8_t kFriendTypeNonMutual = 0x00;
inline constexpr std::uint8_t kFriendTypeMutual    = 0x01;
inline constexpr std::uint8_t kFriendTypeDnd       = 0x02;
inline constexpr std::uint8_t kFriendTypeAway      = 0x04;

inline constexpr std::uint8_t kFriendStatusOffline      = 0x00;
inline constexpr std::uint8_t kFriendStatusOnline       = 0x01;
inline constexpr std::uint8_t kFriendStatusChat         = 0x02;
inline constexpr std::uint8_t kFriendStatusPublicGame   = 0x03;
inline constexpr std::uint8_t kFriendStatusPrivateGame  = 0x05;

}  // namespace pvpgn::protocol::bnet::anongame
