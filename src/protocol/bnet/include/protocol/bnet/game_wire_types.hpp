// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_wire_types.hpp
/// Game list / start / close / map-auth / game-report / join-game
/// wire constants, mirrored from `src/common/bnet_protocol.h`
/// (lines ~2638-3155, 3517-3570). Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::game {

// ---- Packet type codes -------------------------------------------------
inline constexpr std::uint16_t kClientGameListReq    = 0x09ff;
inline constexpr std::uint16_t kServerGameListReply  = 0x09ff;

inline constexpr std::uint16_t kClientStartGame1     = 0x08ff;
inline constexpr std::uint16_t kServerStartGame1Ack  = 0x08ff;
inline constexpr std::uint16_t kClientStartGame3     = 0x1aff;
inline constexpr std::uint16_t kServerStartGame3Ack  = 0x1aff;
inline constexpr std::uint16_t kClientStartGame4     = 0x1cff;
inline constexpr std::uint16_t kServerStartGame4Ack  = 0x1cff;
inline constexpr std::uint16_t kClientUnknown1b      = 0x1bff;

inline constexpr std::uint16_t kClientCloseGame      = 0x02ff;
inline constexpr std::uint16_t kClientCloseGame2     = 0x1fff;

inline constexpr std::uint16_t kClientMapAuthReq1    = 0x32ff;
inline constexpr std::uint16_t kServerMapAuthReply1  = 0x32ff;
inline constexpr std::uint16_t kClientMapAuthReq2    = 0x3cff;
inline constexpr std::uint16_t kServerMapAuthReply2  = 0x3cff;

inline constexpr std::uint16_t kClientGameReport     = 0x2cff;
inline constexpr std::uint16_t kClientJoinGame       = 0x22ff;

inline constexpr std::uint16_t kClientSearchLanGames = 0x2ff7; // w3route class

// ---- GameListReq: gametype filter -------------------------------------
inline constexpr std::uint16_t kGameListReqAll       = 0x0000;
inline constexpr std::uint16_t kGameListReqMelee     = 0x0002;
inline constexpr std::uint16_t kGameListReqFfa       = 0x0003;
inline constexpr std::uint16_t kGameListReqOneOnOne  = 0x0004;
inline constexpr std::uint16_t kGameListReqCtf       = 0x0005;
inline constexpr std::uint16_t kGameListReqGreed     = 0x0006;
inline constexpr std::uint16_t kGameListReqSlaughter = 0x0007;
inline constexpr std::uint16_t kGameListReqSDeath    = 0x0008;
inline constexpr std::uint16_t kGameListReqLadder    = 0x0009;
inline constexpr std::uint16_t kGameListReqIronman   = 0x0010;
inline constexpr std::uint16_t kGameListReqMapset    = 0x000a;
inline constexpr std::uint16_t kGameListReqTeamMelee = 0x000b;
inline constexpr std::uint16_t kGameListReqTeamFfa   = 0x000c;
inline constexpr std::uint16_t kGameListReqTeamCtf   = 0x000d;
inline constexpr std::uint16_t kGameListReqPgl       = 0x000e;
inline constexpr std::uint16_t kGameListReqTopVBot   = 0x000f;
inline constexpr std::uint16_t kGameListReqDiablo    = 0x0409;
inline constexpr std::uint16_t kGameListReqLoaded    = 0x0a00;

// ---- Diablo gametype codes (CLIENT_GAMETYPE_DIABLO_*) -----------------
inline constexpr std::uint32_t kGameTypeDiablo0 = 0x00000000;
inline constexpr std::uint32_t kGameTypeDiablo1 = 0x00000001;
inline constexpr std::uint32_t kGameTypeDiablo2 = 0x00000002;
inline constexpr std::uint32_t kGameTypeDiablo3 = 0x00000003;
inline constexpr std::uint32_t kGameTypeDiablo4 = 0x00000004;
inline constexpr std::uint32_t kGameTypeDiablo5 = 0x00000005;
inline constexpr std::uint32_t kGameTypeDiablo6 = 0x00000006;
inline constexpr std::uint32_t kGameTypeDiablo7 = 0x00000007;
inline constexpr std::uint32_t kGameTypeDiablo8 = 0x00000008;
inline constexpr std::uint32_t kGameTypeDiablo9 = 0x00000009;
inline constexpr std::uint32_t kGameTypeDiabloA = 0x0000000a;
inline constexpr std::uint32_t kGameTypeDiabloB = 0x0000000b;
inline constexpr std::uint32_t kGameTypeDiabloC = 0x0000000c;
inline constexpr std::uint32_t kGameTypeDiabloD = 0x0000000d;

// ---- Diablo II gametype codes -----------------------------------------
inline constexpr std::uint32_t kGameTypeDiablo2Close             = 0x00000000;
inline constexpr std::uint32_t kGameTypeDiablo2OpenNormal        = 0x00000008;
inline constexpr std::uint32_t kGameTypeDiablo2OpenNightmare     = 0x00000009;
inline constexpr std::uint32_t kGameTypeDiablo2OpenHell          = 0x0000000a;
inline constexpr std::uint32_t kGameTypeDiablo2OpenHardcoreNormal    = 0x0000000c;
inline constexpr std::uint32_t kGameTypeDiablo2OpenHardcoreNightmare = 0x0000000d;
inline constexpr std::uint32_t kGameTypeDiablo2OpenHardcoreHell      = 0x0000000e;

// ---- GameListReply server-side game status ---------------------------
inline constexpr std::uint32_t kGameListReplyGameSStatusNotFound      = 0x0;
inline constexpr std::uint32_t kGameListReplyGameSStatusPass          = 0x2;
inline constexpr std::uint32_t kGameListReplyGameSStatusFull          = 0x3;
inline constexpr std::uint32_t kGameListReplyGameSStatusStarted       = 0x4;
inline constexpr std::uint32_t kGameListReplyGameSStatusNoSpawnCdkey  = 0x5;
inline constexpr std::uint32_t kGameListReplyGameSStatusLoaded        = 0x0a00;

inline constexpr std::uint32_t kGameListReplyGameStatusOpen    = 0x00000004;
inline constexpr std::uint32_t kGameListReplyGameStatusFull    = 0x00000006;
inline constexpr std::uint32_t kGameListReplyGameStatusStarted = 0x0000000e;
inline constexpr std::uint32_t kGameListReplyGameStatusDone    = 0x0000000c;
inline constexpr std::uint32_t kGameListReplyGameUnknown6      = 0x0000002b;
inline constexpr std::uint16_t kGameListReplyGameUnknown1      = 0x0001;
inline constexpr std::uint16_t kGameListReplyGameUnknown3      = 0x0002;
inline constexpr std::uint16_t kGameListReplyTypeDiablo2Open   = 0x0704;

// ---- StartGame1 / StartGame3 status codes -----------------------------
inline constexpr std::uint32_t kStartGame1StatusMask    = 0x0000000f;
inline constexpr std::uint32_t kStartGame1StatusOpen    = 0x00000004;
inline constexpr std::uint32_t kStartGame1StatusFull    = 0x00000006;
inline constexpr std::uint32_t kStartGame1StatusStarted = 0x0000000e;
inline constexpr std::uint32_t kStartGame1StatusDone    = 0x0000000c;

inline constexpr std::uint32_t kStartGame3StatusMask    = 0x0000000f;
inline constexpr std::uint32_t kStartGame3StatusOpen1   = 0x00000001;
inline constexpr std::uint32_t kStartGame3StatusOpen    = 0x00000004;
inline constexpr std::uint32_t kStartGame3StatusFull    = 0x00000006;
inline constexpr std::uint32_t kStartGame3StatusStarted = 0x0000000e;
inline constexpr std::uint32_t kStartGame3StatusDone    = 0x0000000c;

// ---- StartGame4 (BW/SC newer) status + option bits --------------------
inline constexpr std::uint32_t kStartGame4StatusMask16        = 0x000000ff;
inline constexpr std::uint32_t kStartGame4StatusMaskInitValid = 0x00000093;
inline constexpr std::uint32_t kStartGame4StatusMaskOpenValid = 0x0000009f;
inline constexpr std::uint32_t kStartGame4StatusInit          = 0x00000000;
inline constexpr std::uint32_t kStartGame4StatusPrivate       = 0x00000001;
inline constexpr std::uint32_t kStartGame4StatusFull          = 0x00000002;
inline constexpr std::uint32_t kStartGame4StatusOpen          = 0x00000004;
inline constexpr std::uint32_t kStartGame4StatusStart         = 0x00000008;
inline constexpr std::uint32_t kStartGame4StatusDiscIsLoss    = 0x00000010;
inline constexpr std::uint32_t kStartGame4StatusReplay        = 0x00000080;
inline constexpr std::uint16_t kStartGame4FlagPrivate         = 0x0001;

// ---- StartGameX_ACK reply codes ---------------------------------------
inline constexpr std::uint32_t kStartGame1AckNo = 0x00000000;
inline constexpr std::uint32_t kStartGame1AckOk = 0x00000001;
inline constexpr std::uint32_t kStartGame3AckNo = 0x00000000;
inline constexpr std::uint32_t kStartGame3AckOk = 0x00000001;
inline constexpr std::uint32_t kStartGame4AckNo = 0x00000001;
inline constexpr std::uint32_t kStartGame4AckOk = 0x00000000;

// ---- MapType / GameSpeed / Tileset / Difficulty (D2) ------------------
inline constexpr std::uint32_t kMapTypeSelfmade = 0;
inline constexpr std::uint32_t kMapTypeBlizzard = 1;
inline constexpr std::uint32_t kMapTypeLadder   = 2;
inline constexpr std::uint32_t kMapTypePgl      = 3;
inline constexpr std::uint32_t kMapTypeKbk      = 4;
inline constexpr std::uint32_t kMapTypeCompUsa  = 5;

inline constexpr std::uint32_t kGameSpeedSlowest = 0;
inline constexpr std::uint32_t kGameSpeedSlower  = 1;
inline constexpr std::uint32_t kGameSpeedSlow    = 2;
inline constexpr std::uint32_t kGameSpeedNormal  = 3;
inline constexpr std::uint32_t kGameSpeedFast    = 4;
inline constexpr std::uint32_t kGameSpeedFaster  = 5;
inline constexpr std::uint32_t kGameSpeedFastest = 6;

inline constexpr std::uint32_t kTilesetBadlands     = 0;
inline constexpr std::uint32_t kTilesetSpace        = 1;
inline constexpr std::uint32_t kTilesetInstallation = 2;
inline constexpr std::uint32_t kTilesetAshworld     = 3;
inline constexpr std::uint32_t kTilesetJungle       = 4;
inline constexpr std::uint32_t kTilesetDesert       = 5;
inline constexpr std::uint32_t kTilesetIce          = 6;
inline constexpr std::uint32_t kTilesetTwilight     = 7;

inline constexpr std::uint32_t kDifficultyNormal             = 1;
inline constexpr std::uint32_t kDifficultyNightmare          = 2;
inline constexpr std::uint32_t kDifficultyHell               = 3;
inline constexpr std::uint32_t kDifficultyHardcoreNormal     = 4;
inline constexpr std::uint32_t kDifficultyHardcoreNightmare  = 5;
inline constexpr std::uint32_t kDifficultyHardcoreHell       = 6;

// ---- MapAuthReply1 / MapAuthReply2 response codes ---------------------
inline constexpr std::uint32_t kMapAuthReply1No        = 0x00000000;
inline constexpr std::uint32_t kMapAuthReply1Ok        = 0x00000001;
inline constexpr std::uint32_t kMapAuthReply1LadderOk  = 0x00000002;
inline constexpr std::uint32_t kMapAuthReply2No        = 0x00000000;
inline constexpr std::uint32_t kMapAuthReply2Ok        = 0x00000001;
inline constexpr std::uint32_t kMapAuthReply2LadderOk  = 0x00000002;

// ---- Unknown_1B placeholders ------------------------------------------
inline constexpr std::uint16_t kUnknown1bUnknown1 = 0x0002;
inline constexpr std::uint32_t kUnknown1bUnknown2 = 0x00000000;
inline constexpr std::uint32_t kUnknown1bUnknown3 = 0x00000000;

} // namespace pvpgn::protocol::bnet::game
