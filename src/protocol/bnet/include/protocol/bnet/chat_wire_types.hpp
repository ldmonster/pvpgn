// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file chat_wire_types.hpp
/// Channel, server-list, chat-message, player-info, profile, stats,
/// ladder, icon-list, D2 character-info wire constants, mirrored from
/// `src/common/bnet_protocol.h`. Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::chat {

// ---- Packet type codes -------------------------------------------------
inline constexpr std::uint16_t kClientStatsReq         = 0x26ff;
inline constexpr std::uint16_t kServerStatsReply       = 0x26ff;
inline constexpr std::uint16_t kClientPlayerInfoReq    = 0x0aff;
inline constexpr std::uint16_t kServerPlayerInfoReply  = 0x0aff;
inline constexpr std::uint16_t kClientProgIdent2       = 0x0bff;
inline constexpr std::uint16_t kClientJoinChannel      = 0x0cff;
inline constexpr std::uint16_t kServerChannelList      = 0x0bff;
inline constexpr std::uint16_t kServerServerList       = 0x04ff;
inline constexpr std::uint16_t kServerMessage          = 0x0fff;
inline constexpr std::uint16_t kClientMessage          = 0x0eff;
inline constexpr std::uint16_t kClientLeaveChannel     = 0x10ff;

inline constexpr std::uint16_t kClientProfileReq       = 0x35ff;
inline constexpr std::uint16_t kServerProfileReply     = 0x35ff;
inline constexpr std::uint16_t kClientUnknown37        = 0x37ff;
inline constexpr std::uint16_t kServerUnknown37        = 0x37ff;
inline constexpr std::uint16_t kClientUnknown39        = 0x39ff;

inline constexpr std::uint16_t kClientLadderSearchReq  = 0x2fff;
inline constexpr std::uint16_t kServerLadderSearchReply = 0x2fff;
inline constexpr std::uint16_t kClientLadderReq        = 0x2eff;
inline constexpr std::uint16_t kServerLadderReply      = 0x2eff;
inline constexpr std::uint16_t kClientStatsUpdate      = 0x27ff;

// ---- JoinChannel flags ------------------------------------------------
inline constexpr std::uint32_t kJoinChannelNormal  = 0x00000000;
inline constexpr std::uint32_t kJoinChannelGeneric = 0x00000001;
inline constexpr std::uint32_t kJoinChannelCreate  = 0x00000002;

// ---- ServerMessage magic + message types ------------------------------
inline constexpr std::uint32_t kServerMessagePlayerIpDummy = 0x00000000;
inline constexpr std::uint32_t kServerMessageRegAuth       = 0xBAADF00D;
inline constexpr std::uint32_t kServerMessageAccountNum    = 0x0df0adba;
inline constexpr std::uint32_t kServerMessageUnknown1      = 0x00000000; // ServerServerList unknown1 / ChangePassAck.fail

inline constexpr std::uint32_t kServerMessageTypeAddUser             = 0x00000001;
inline constexpr std::uint32_t kServerMessageTypeJoin                = 0x00000002;
inline constexpr std::uint32_t kServerMessageTypePart                = 0x00000003;
inline constexpr std::uint32_t kServerMessageTypeWhisper             = 0x00000004;
inline constexpr std::uint32_t kServerMessageTypeTalk                = 0x00000005;
inline constexpr std::uint32_t kServerMessageTypeBroadcast           = 0x00000006;
inline constexpr std::uint32_t kServerMessageTypeChannel             = 0x00000007;
inline constexpr std::uint32_t kServerMessageTypeUserFlags           = 0x00000009;
inline constexpr std::uint32_t kServerMessageTypeWhisperAck          = 0x0000000a;
inline constexpr std::uint32_t kServerMessageTypeChannelFull         = 0x0000000d;
inline constexpr std::uint32_t kServerMessageTypeChannelDoesNotExist = 0x0000000e;
inline constexpr std::uint32_t kServerMessageTypeChannelRestricted   = 0x0000000f;
inline constexpr std::uint32_t kServerMessageTypeInfo                = 0x00000012;
inline constexpr std::uint32_t kServerMessageTypeError               = 0x00000013;
inline constexpr std::uint32_t kServerMessageTypeEmote               = 0x00000017;

// ---- Player flags (per-user icon flags in SERVER_MESSAGE) -------------
inline constexpr std::uint32_t kMfBlizzard = 0x00000001;
inline constexpr std::uint32_t kMfGavel    = 0x00000002;
inline constexpr std::uint32_t kMfVoice    = 0x00000004;
inline constexpr std::uint32_t kMfBnet     = 0x00000008;
inline constexpr std::uint32_t kMfPlug     = 0x00000010;
inline constexpr std::uint32_t kMfX        = 0x00000020;
inline constexpr std::uint32_t kMfShades   = 0x00000040;
inline constexpr std::uint32_t kMfBeep     = 0x00000100;
inline constexpr std::uint32_t kMfPglPlay  = 0x00000200;
inline constexpr std::uint32_t kMfPglOffl  = 0x00000400;
inline constexpr std::uint32_t kMfKbkPlay  = 0x00000800;
inline constexpr std::uint32_t kMfKbkRef   = 0x00001000;

// ---- Channel flags (for MT_CHANNEL messages) --------------------------
inline constexpr std::uint32_t kCfPublic     = 0x00000001;
inline constexpr std::uint32_t kCfModerated  = 0x00000002;
inline constexpr std::uint32_t kCfRestricted = 0x00000004;
inline constexpr std::uint32_t kCfTheVoid    = 0x00000008;
inline constexpr std::uint32_t kCfSystem     = 0x00000020;
inline constexpr std::uint32_t kCfOfficial   = 0x00001000;

// ---- PlayerInfo: DRTL class enum --------------------------------------
inline constexpr std::uint32_t kPlayerInfoDrtlClassWarrior  = 0;
inline constexpr std::uint32_t kPlayerInfoDrtlClassRogue    = 1;
inline constexpr std::uint32_t kPlayerInfoDrtlClassSorcerer = 2;

// ---- LadderSearchReq id / type ----------------------------------------
inline constexpr std::uint32_t kLadderSearchReqIdStandard       = 0x00000001;
inline constexpr std::uint32_t kLadderSearchReqIdIronman        = 0x00000003;
inline constexpr std::uint32_t kLadderSearchReqTypeHighestRated = 0x00000000;
inline constexpr std::uint32_t kLadderSearchReqTypeMostWins     = 0x00000002;
inline constexpr std::uint32_t kLadderSearchReqTypeMostGames    = 0x00000003;
inline constexpr std::uint32_t kLadderSearchReplyRankNone       = 0xffffffff;

// ---- D2 character-info bytes (UNKNOWN_37 payload) ---------------------
inline constexpr std::uint8_t kD2CharInfoUnknownB1     = 0x83;
inline constexpr std::uint8_t kD2CharInfoUnknownB2     = 0x80;
inline constexpr std::uint8_t kD2CharInfoFiller        = 0xff;
inline constexpr std::uint8_t kD2CharInfoClassAmazon      = 0x01;
inline constexpr std::uint8_t kD2CharInfoClassSorceress   = 0x02;
inline constexpr std::uint8_t kD2CharInfoClassNecromancer = 0x03;
inline constexpr std::uint8_t kD2CharInfoClassPaladin     = 0x04;
inline constexpr std::uint8_t kD2CharInfoClassBarbarian   = 0x05;
inline constexpr std::uint8_t kD2CharInfoClassDruid       = 0x06;
inline constexpr std::uint8_t kD2CharInfoClassAssassin    = 0x07;

// ---- W3 race / icon (used in stats updates & MOTD) --------------------
inline constexpr std::uint32_t kW3RaceRandom    = 32;
inline constexpr std::uint32_t kW3RaceHumans    = 1;
inline constexpr std::uint32_t kW3RaceOrcs      = 2;
inline constexpr std::uint32_t kW3RaceUndead    = 8;
inline constexpr std::uint32_t kW3RaceNightelves = 4;
inline constexpr std::uint32_t kW3RaceDemons    = 16;

inline constexpr std::uint32_t kW3IconRandom     = 0;
inline constexpr std::uint32_t kW3IconHumans     = 1;
inline constexpr std::uint32_t kW3IconOrcs       = 2;
inline constexpr std::uint32_t kW3IconUndead     = 3;
inline constexpr std::uint32_t kW3IconNightelves = 4;
inline constexpr std::uint32_t kW3IconDemons     = 5;

} // namespace pvpgn::protocol::bnet::chat
