// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// IRC protocol numeric reply / error codes, mirrored from
/// `src/common/irc_protocol.h`.
///
/// The IRC protocol is text-based; there are no binary wire structs.
/// This header exposes only the numeric reply codes (1-399) and error
/// codes (400-599+), the WOL (Westwood Online) extensions, and a
/// handful of structural constants used by the codec layer.

#include <cstdint>

namespace pvpgn::protocol::irc::wire {

// ---- Structural constants ---------------------------------------------

inline constexpr int         kWolNicknameLen = 9;
inline constexpr const char* kChannelPrefix  = "(ohv)@%+";
inline constexpr const char* kChannelType    = "#";

// ---- RPL_* numeric reply codes (1..399) -------------------------------

inline constexpr int kRplWelcome           = 1;
inline constexpr int kRplYourHost          = 2;
inline constexpr int kRplCreated           = 3;
inline constexpr int kRplMyInfo            = 4;
inline constexpr int kRplISupport          = 5;
inline constexpr int kRplSnomask           = 8;
inline constexpr int kRplStatMemTot        = 9;
inline constexpr int kRplStatMem           = 10;
inline constexpr int kRplMap               = 15;
inline constexpr int kRplMapMore           = 16;
inline constexpr int kRplMapEnd            = 17;

inline constexpr int kRplTraceLink         = 200;
inline constexpr int kRplTraceConnecting   = 201;
inline constexpr int kRplTraceHandshake    = 202;
inline constexpr int kRplTraceUnknown      = 203;
inline constexpr int kRplTraceOperator     = 204;
inline constexpr int kRplTraceUser         = 205;
inline constexpr int kRplTraceServer       = 206;
inline constexpr int kRplTraceNewType      = 208;
inline constexpr int kRplTraceClass        = 209;
inline constexpr int kRplStatsLinkInfo     = 211;
inline constexpr int kRplStatsCommands     = 212;
inline constexpr int kRplStatsCLine        = 213;
inline constexpr int kRplStatsNLine        = 214;
inline constexpr int kRplStatsILine        = 215;
inline constexpr int kRplStatsKLine        = 216;
inline constexpr int kRplStatsPLine        = 217;
inline constexpr int kRplStatsYLine        = 218;
inline constexpr int kRplEndOfStats        = 219;
inline constexpr int kRplUmodeIs           = 221;
inline constexpr int kRplServiceInfo       = 231;
inline constexpr int kRplEndOfServices     = 232;
inline constexpr int kRplService           = 233;
inline constexpr int kRplServList          = 234;
inline constexpr int kRplServListEnd       = 235;
inline constexpr int kRplStatsEngine       = 237;
inline constexpr int kRplStatsFLine        = 238;
inline constexpr int kRplStatsLLine        = 241;
inline constexpr int kRplStatsUptime       = 242;
inline constexpr int kRplStatsOLine        = 243;
inline constexpr int kRplStatsHLine        = 244;
inline constexpr int kRplStatsTLine        = 246;
inline constexpr int kRplStatsGLine        = 247;
inline constexpr int kRplStatsULine        = 248;
inline constexpr int kRplStatsDebug        = 249;
inline constexpr int kRplStatsConn         = 250;
inline constexpr int kRplLUserClient       = 251;
inline constexpr int kRplLUserOp           = 252;
inline constexpr int kRplLUserUnknown      = 253;
inline constexpr int kRplLUserChannels     = 254;
inline constexpr int kRplLUserMe           = 255;
inline constexpr int kRplAdminMe           = 256;
inline constexpr int kRplAdminLoc1         = 257;
inline constexpr int kRplAdminLoc2         = 258;
inline constexpr int kRplAdminEmail        = 259;
inline constexpr int kRplTraceLog          = 261;
inline constexpr int kRplTracePing         = 262;
inline constexpr int kRplPrivs             = 270;
inline constexpr int kRplSileList          = 271;
inline constexpr int kRplEndOfSileList     = 272;
inline constexpr int kRplStatsDLine        = 275;

inline constexpr int kRplGList             = 280;
inline constexpr int kRplEndOfGList        = 281;
inline constexpr int kRplJupeList          = 282;
inline constexpr int kRplEndOfJupeList     = 283;
inline constexpr int kRplFeature           = 284;

inline constexpr int kRplNone              = 300;
inline constexpr int kRplAway              = 301;
inline constexpr int kRplUserHost          = 302;
inline constexpr int kRplIson              = 303;
inline constexpr int kRplText              = 304;
inline constexpr int kRplUnAway            = 305;
inline constexpr int kRplNowAway           = 306;
inline constexpr int kRplUserIp            = 307;
inline constexpr int kRplWhoisUser         = 311;
inline constexpr int kRplWhoisServer       = 312;
inline constexpr int kRplWhoisOperator     = 313;
inline constexpr int kRplWhoWasUser        = 314;
inline constexpr int kRplEndOfWho          = 315;
inline constexpr int kRplWhoisIdle         = 317;
inline constexpr int kRplEndOfWhois        = 318;
inline constexpr int kRplWhoisChannels     = 319;
inline constexpr int kRplListStart         = 321;
inline constexpr int kRplList              = 322;
inline constexpr int kRplListEnd           = 323;
inline constexpr int kRplChannelModeIs     = 324;
inline constexpr int kRplCreationTime      = 329;
inline constexpr int kRplNoTopic           = 331;
inline constexpr int kRplTopic             = 332;
inline constexpr int kRplTopicWhoTime      = 333;
inline constexpr int kRplListUsage         = 334;
inline constexpr int kRplInviting          = 341;
inline constexpr int kRplInviteList        = 346;
inline constexpr int kRplEndOfInviteList   = 347;
inline constexpr int kRplVersion           = 351;
inline constexpr int kRplWhoReply          = 352;
inline constexpr int kRplNamReply          = 353;
inline constexpr int kRplWhoSpcRpl         = 354;
inline constexpr int kRplKillDone          = 361;
inline constexpr int kRplClosing           = 362;
inline constexpr int kRplCloseEnd          = 363;
inline constexpr int kRplLinks             = 364;
inline constexpr int kRplEndOfLinks        = 365;
inline constexpr int kRplEndOfNames        = 366;
inline constexpr int kRplBanList           = 367;
inline constexpr int kRplEndOfBanList      = 368;
inline constexpr int kRplEndOfWhoWas       = 369;
inline constexpr int kRplInfo              = 371;
inline constexpr int kRplMotd              = 372;
inline constexpr int kRplInfoStart         = 373;
inline constexpr int kRplEndOfInfo         = 374;
inline constexpr int kRplMotdStart         = 375;
inline constexpr int kRplEndOfMotd         = 376;
inline constexpr int kRplYoureOper         = 381;
inline constexpr int kRplRehashing         = 382;
inline constexpr int kRplMyPortIs          = 384;
inline constexpr int kRplNotOperAnymore    = 385;
inline constexpr int kRplTime              = 391;

// ---- WOL (Westwood Online) extensions ---------------------------------

inline constexpr int kRplGetLocale       = 309;
inline constexpr int kRplSetLocale       = 310;
inline constexpr int kRplGameChannel     = 326;
inline constexpr int kRplChannel         = 327;
inline constexpr int kRplGetCodepage     = 328;
inline constexpr int kRplSetCodepage     = 329;
inline constexpr int kRplGetBuddy        = 333;
inline constexpr int kRplAddBuddy        = 334;
inline constexpr int kRplDelBuddy        = 335;
inline constexpr int kRplBattleClan      = 358;
inline constexpr int kRplAnnounce        = 377;   // WOLv2 only
inline constexpr int kRplBadLogin        = 378;
inline constexpr int kRplVerchkNonreq    = 379;
inline constexpr int kRplFindUser        = 388;
inline constexpr int kRplPage            = 389;
inline constexpr int kRplFindUserEx      = 398;
inline constexpr int kRplGetInsider      = 399;

// WOL servserv replies (600-615)
inline constexpr int kRplUpdateNonex     = 602;
inline constexpr int kRplUpdateExist     = 603;
inline constexpr int kRplWolServ         = 605;
inline constexpr int kRplUpdateFtp       = 606;
inline constexpr int kRplQuit            = 607;
inline constexpr int kRplGameresServ     = 608;
inline constexpr int kRplLadderServ      = 609;
inline constexpr int kRplLobCount        = 610;
inline constexpr int kRplWdtServ         = 611;
inline constexpr int kRplManglerServ     = 612;
inline constexpr int kRplTicketServ      = 613;
inline constexpr int kRplPingServer      = 615;

// WOL custom errors
inline constexpr int kErrIdNoExist       = 439;
inline constexpr int kErrGameHasClosed   = 478;

// ---- ERR_* numeric error codes (400..599) -----------------------------

inline constexpr int kErrFirstError       = 400;
inline constexpr int kErrNoSuchNick       = 401;
inline constexpr int kErrNoSuchServer     = 402;
inline constexpr int kErrNoSuchChannel    = 403;
inline constexpr int kErrCannotSendToChan = 404;
inline constexpr int kErrTooManyChannels  = 405;
inline constexpr int kErrWasNoSuchNick    = 406;
inline constexpr int kErrTooManyTargets   = 407;
inline constexpr int kErrNoOrigin         = 409;
inline constexpr int kErrNoRecipient      = 411;
inline constexpr int kErrNoTextToSend     = 412;
inline constexpr int kErrNoTopLevel       = 413;
inline constexpr int kErrWildTopLevel     = 414;
inline constexpr int kErrQueryTooLong     = 416;
inline constexpr int kErrUnknownCommand   = 421;
inline constexpr int kErrNoMotd           = 422;
inline constexpr int kErrNoAdminInfo      = 423;
inline constexpr int kErrNoNicknameGiven  = 431;
inline constexpr int kErrErroneusNickname = 432;
inline constexpr int kErrNicknameInUse    = 433;
inline constexpr int kErrNickCollision    = 436;
inline constexpr int kErrBanNickChange    = 437;
inline constexpr int kErrNickTooFast      = 438;
inline constexpr int kErrTargetTooFast    = 439;
inline constexpr int kErrUserNotInChannel = 441;
inline constexpr int kErrNotOnChannel     = 442;
inline constexpr int kErrUserOnChannel    = 443;
inline constexpr int kErrNotRegistered    = 451;
inline constexpr int kErrNeedMoreParams   = 461;
inline constexpr int kErrAlreadyRegistred = 462;
inline constexpr int kErrNoPermForHost    = 463;
inline constexpr int kErrPasswdMismatch   = 464;
inline constexpr int kErrYoureBannedCreep = 465;
inline constexpr int kErrYouWillBeBanned  = 466;
inline constexpr int kErrKeySet           = 467;
inline constexpr int kErrInvalidUsername  = 468;
inline constexpr int kErrChannelIsFull    = 471;
inline constexpr int kErrUnknownMode      = 472;
inline constexpr int kErrInviteOnlyChan   = 473;
inline constexpr int kErrBannedFromChan   = 474;
inline constexpr int kErrBadChannelKey    = 475;
inline constexpr int kErrBadChanMask      = 476;
inline constexpr int kErrBanListFull      = 478;
inline constexpr int kErrBadChanName      = 479;
inline constexpr int kErrNoPrivileges     = 481;
inline constexpr int kErrChanOPrivsNeeded = 482;
inline constexpr int kErrCantKillServer   = 483;
inline constexpr int kErrIsChanService    = 484;
inline constexpr int kErrRestricted       = 484;
inline constexpr int kErrVoiceNeeded      = 489;
inline constexpr int kErrNoOperHost       = 491;
inline constexpr int kErrNoFeature        = 493;
inline constexpr int kErrBadFeatValue     = 494;
inline constexpr int kErrBadLogType       = 495;
inline constexpr int kErrBadLogSys        = 496;
inline constexpr int kErrBadLogValue      = 497;
inline constexpr int kErrIsOperLChan      = 498;
inline constexpr int kErrUModeUnknownFlag = 501;
inline constexpr int kErrUsersDontMatch   = 502;
inline constexpr int kErrSileListFull     = 511;
inline constexpr int kErrNoSuchGLine      = 512;
inline constexpr int kErrBadPing          = 513;
inline constexpr int kErrNoSuchJupe       = 514;
inline constexpr int kErrBadExpire        = 515;
inline constexpr int kErrDontCheat        = 516;
inline constexpr int kErrDisabled         = 517;
inline constexpr int kErrLongMask         = 518;
inline constexpr int kErrTooManyUsers     = 519;
inline constexpr int kErrMaskTooWide      = 520;
inline constexpr int kErrLastError        = 521;

}  // namespace pvpgn::protocol::irc::wire
