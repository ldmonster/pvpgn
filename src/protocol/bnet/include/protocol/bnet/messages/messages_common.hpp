// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_common.hpp
/// SID constants and Null/Ping messages.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace pvpgn::protocol::bnet {

namespace pvpgn::protocol::bnet {

// SID codes (subset; matches legacy bnet_protocol.h).
inline constexpr std::uint8_t kSidNull            = 0x00;
inline constexpr std::uint8_t kSidGetAdvListEx    = 0x09;  // GAMELISTREQ/REPLY
inline constexpr std::uint8_t kSidEnterChat       = 0x0A;
inline constexpr std::uint8_t kSidJoinChannel     = 0x0C;
inline constexpr std::uint8_t kSidChatCommand     = 0x0E;
inline constexpr std::uint8_t kSidChatEvent       = 0x0F;
inline constexpr std::uint8_t kSidPing            = 0x25;  // ECHOREQ / ECHOREPLY
inline constexpr std::uint8_t kSidReadUserData    = 0x26;  // READUSERDATA / STATSREQ-REPLY
inline constexpr std::uint8_t kSidWriteUserData   = 0x27;  // WRITEUSERDATA / STATSUPDATE
inline constexpr std::uint8_t kSidLadderSearch    = 0x2F;  // LADDERSEARCHREQ/REPLY
inline constexpr std::uint8_t kSidIconReq         = 0x2D;  // CLIENT_ICONREQ / SERVER_ICONREPLY
inline constexpr std::uint8_t kSidGetFileTime     = 0x33;  // FILEINFOREQ/REPLY
inline constexpr std::uint8_t kSidCdKey2          = 0x36;  // CDKEY2 / CDKEYREPLY2
inline constexpr std::uint8_t kSidLogonResponse2  = 0x3A;
inline constexpr std::uint8_t kSidAuthInfo        = 0x50;
inline constexpr std::uint8_t kSidAuthCheck       = 0x51;
inline constexpr std::uint8_t kSidFriendsList     = 0x65;  // FRIENDSLISTREQ/REPLY
inline constexpr std::uint8_t kSidFriendInfo      = 0x66;  // FRIENDINFOREQ/REPLY
inline constexpr std::uint8_t kSidFriendAdd       = 0x67;  // SERVER_FRIENDADD_ACK
inline constexpr std::uint8_t kSidFriendDel       = 0x68;  // SERVER_FRIENDDEL_ACK
inline constexpr std::uint8_t kSidFriendMove      = 0x69;  // SERVER_FRIENDMOVE_ACK
inline constexpr std::uint8_t kSidArrangedTeamFriendScreen = 0x60;  // ARRANGEDTEAM_FRIENDSCREEN req/reply
inline constexpr std::uint8_t kSidArrangedTeamInviteFriend = 0x61;  // ARRANGEDTEAM_INVITE_FRIEND req/ack
inline constexpr std::uint8_t kSidArrangedTeamMemberDecline = 0x62;  // ARRANGEDTEAM_MEMBER_DECLINE (server)
inline constexpr std::uint8_t kSidArrangedTeamSendInvite   = 0x63;  // server-send invite / client accept-decline
inline constexpr std::uint8_t kSidArrangedTeamAcceptInvite = 0xFD;  // CLIENT_ARRANGEDTEAM_ACCEPT_INVITE (empty)
inline constexpr std::uint8_t kSidStartGame4  = 0x1C;  // SID_STARTADVEX3 (host game / ack)
inline constexpr std::uint8_t kSidUdpOk       = 0x14;  // CLIENT_UDPOK (echo confirmation)
inline constexpr std::uint8_t kSidLadderList  = 0x2E;  // CLIENT_LADDERREQ / SERVER_LADDERREPLY
inline constexpr std::uint8_t kSidCheckAd     = 0x15;  // CLIENT_ADREQ / SERVER_ADREPLY
inline constexpr std::uint8_t kSidAdClick     = 0x16;  // CLIENT_ADCLICK
inline constexpr std::uint8_t kSidAdAck       = 0x21;  // CLIENT_ADACK
inline constexpr std::uint8_t kSidAdClick2    = 0x41;  // CLIENT_ADCLICK2 / SERVER_ADCLICKREPLY2
inline constexpr std::uint8_t kSidMotd        = 0x46;  // CLIENT_MOTDREQ / SERVER_MOTD_W3
inline constexpr std::uint8_t kSidChannelList = 0x0B;  // CLIENT_PROGIDENT2 / SERVER_CHANNELLIST
inline constexpr std::uint8_t kSidLeaveChat   = 0x10;  // CLIENT_LEAVECHANNEL (empty body)
inline constexpr std::uint8_t kSidRegSnoop    = 0x18;  // SERVER_REGSNOOPREQ / CLIENT_REGSNOOPREPLY
inline constexpr std::uint8_t kSidProfile     = 0x35;  // CLIENT_PROFILEREQ / SERVER_PROFILEREPLY
inline constexpr std::uint8_t kSidSetEmail    = 0x59;  // SERVER_SETEMAILREQ / CLIENT_SETEMAILREPLY
inline constexpr std::uint8_t kSidGetPassword = 0x5A;  // CLIENT_GETPASSWORDREQ
inline constexpr std::uint8_t kSidChangeEmail = 0x5B;  // CLIENT_CHANGEEMAILREQ
inline constexpr std::uint8_t kSidCrashDump   = 0x5D;  // CLIENT_CRASHDUMP
inline constexpr std::uint8_t kSidCharList    = 0x37;  // CLIENT_UNKNOWN_37 / SERVER_UNKNOWN_37 — legacy D2 char list
inline constexpr std::uint8_t kSidServerList  = 0x04;  // SERVER_SERVERLIST — alt-server fallback list
inline constexpr std::uint8_t kSidMessageBox  = 0x19;  // SERVER_MESSAGEBOX — modal dialog push
inline constexpr std::uint8_t kSidRealmList   = 0x40;  // CLIENT_REALMLISTREQ_110 / SERVER_REALMLISTREPLY_110
inline constexpr std::uint8_t kSidRealmJoin   = 0x3E;  // CLIENT_REALMJOINREQ_109 / SERVER_REALMJOINREPLY_109
inline constexpr std::uint8_t kSidWarcraftGeneral = 0x44;  // CLIENT_FINDANONGAME / SERVER_ANONGAME_* (sub-option multiplexer)
inline constexpr std::uint8_t kSidExtraWork      = 0x4B;  // CLIENT_EXTRAWORK
inline constexpr std::uint8_t kSidRequiredWork   = 0x4C;  // SERVER_REQUIREDWORK
inline constexpr std::uint8_t kSidRealmListLegacy = 0x34;  // CLIENT_REALMLISTREQ / SERVER_REALMLISTREPLY (pre-1.10)
inline constexpr std::uint8_t kSidCdKey3      = 0x42;  // CLIENT_CDKEY3 / SERVER_CDKEYREPLY3 (multi-key auth)
inline constexpr std::uint8_t kSidCreateAccount2 = 0x52;  // CLIENT_CREATEACCOUNT_W3 / SERVER_CREATEACCOUNT_W3 (NLS create)
inline constexpr std::uint8_t kSidLoginW3        = 0x53;  // CLIENT_LOGINREQ_W3 / SERVER_LOGINREPLY_W3 (NLS step A)
inline constexpr std::uint8_t kSidLogonProofW3   = 0x54;  // CLIENT_LOGONPROOFREQ / SERVER_LOGONPROOFREPLY (NLS step B)
inline constexpr std::uint8_t kSidPassChange     = 0x55;  // CLIENT_PASSCHANGEREQ / SERVER_PASSCHANGEREPLY (NLS pwchg step A)
inline constexpr std::uint8_t kSidPassChangeProof = 0x56; // CLIENT_PASSCHANGEPROOFREQ / SERVER_PASSCHANGEPROOFREPLY (NLS pwchg step B)
inline constexpr std::uint8_t kSidClanCreate         = 0x70;  // CLAN_CREATEREQ/REPLY
inline constexpr std::uint8_t kSidClanCreateInvite   = 0x71;  // CLAN_CREATEINVITE_REQ (client) / REPLY (server)
inline constexpr std::uint8_t kSidClanCreateInvite2  = 0x72;  // CLAN_CREATEINVITE_REQ (server)/ REPLY (client)
inline constexpr std::uint8_t kSidClanDisband        = 0x73;  // CLAN_DISBANDREQ/REPLY
inline constexpr std::uint8_t kSidClanMemberNewChief = 0x74;  // CLAN_MEMBERNEWCHIEFREQ/REPLY
inline constexpr std::uint8_t kSidClanClanAck        = 0x75;  // SERVER_CLAN_CLANACK (server only)
inline constexpr std::uint8_t kSidClanQuitNotify     = 0x76;  // SERVER_CLANQUITNOTIFY (server only)
inline constexpr std::uint8_t kSidClanInvite         = 0x77;  // CLAN_INVITEREQ/REPLY
inline constexpr std::uint8_t kSidClanMemberRemove   = 0x78;  // CLANMEMBER_REMOVE_REQ/REPLY
inline constexpr std::uint8_t kSidClanMemberRankUpdate = 0x7A;  // CLANMEMBER_RANKUPDATE_REQ/REPLY
inline constexpr std::uint8_t kSidClanMotdChange     = 0x7B;  // CLAN_MOTDCHG (client only)
inline constexpr std::uint8_t kSidClanMotd           = 0x7C;  // CLAN_MOTDREQ/REPLY
inline constexpr std::uint8_t kSidClanInvite2        = 0x79;  // CLAN_INVITE2 (late-join invite, both directions)
inline constexpr std::uint8_t kSidClanMemberList     = 0x7D;  // CLANMEMBERLIST_REQ/REPLY
inline constexpr std::uint8_t kSidClanMemberRemoved  = 0x7E;  // SERVER_CLANMEMBER_REMOVED_NOTIFY (server only)
inline constexpr std::uint8_t kSidClanMemberUpdate   = 0x7F;  // SERVER_CLANMEMBERUPDATE (server only)
inline constexpr std::uint8_t kSidClanInfo        = 0x82;  // CLANINFOREQ/REPLY

// Legacy / OLS (Old Login System) SIDs — added in the "implement all SIDs" pass.
inline constexpr std::uint8_t kSidCompInfo1      = 0x05;  // CLIENT_COMPINFO1 / SERVER_COMPREPLY
inline constexpr std::uint8_t kSidProgIdent      = 0x06;  // CLIENT_PROGIDENT / SERVER_AUTHREQ1
inline constexpr std::uint8_t kSidAuthReq1       = 0x07;  // CLIENT_AUTHREQ1 / SERVER_AUTHREPLY1
inline constexpr std::uint8_t kSidCountryInfo1   = 0x12;  // CLIENT_COUNTRYINFO1
inline constexpr std::uint8_t kSidSessionKey2    = 0x1D;  // SERVER_SESSIONKEY2
inline constexpr std::uint8_t kSidCompInfo2      = 0x1E;  // CLIENT_COMPINFO2
inline constexpr std::uint8_t kSidSessionKey1    = 0x28;  // SERVER_SESSIONKEY1
inline constexpr std::uint8_t kSidLogonResponse  = 0x29;  // CLIENT_LOGINREQ1 / SERVER_LOGINREPLY1 (OLS)
inline constexpr std::uint8_t kSidCreateAccount1 = 0x2A;  // CLIENT_CREATEACCTREQ1 / SERVER_CREATEACCTREPLY1
inline constexpr std::uint8_t kSidUnknown2B      = 0x2B;  // CLIENT_UNKNOWN_2B
inline constexpr std::uint8_t kSidCdKeyLegacy    = 0x30;  // CLIENT_CDKEY / SERVER_CDKEYREPLY
inline constexpr std::uint8_t kSidChangePassword = 0x31;  // CLIENT_CHANGEPASSREQ / SERVER_CHANGEPASSACK
inline constexpr std::uint8_t kSidUnknown39      = 0x39;  // CLIENT_UNKNOWN_39 (post-delete advisory)
inline constexpr std::uint8_t kSidCreateAccount  = 0x3D;  // CLIENT_CREATEACCTREQ2 / SERVER_CREATEACCTREPLY2
inline constexpr std::uint8_t kSidNetGamePort    = 0x45;  // CLIENT_CHANGEGAMEPORT

// Game-lifecycle SIDs (host/join/close/report).
inline constexpr std::uint8_t kSidCloseGame      = 0x02;  // CLIENT_CLOSEGAME (empty body)
inline constexpr std::uint8_t kSidStartGame1     = 0x08;  // CLIENT_STARTGAME1 / SERVER_STARTGAME1_ACK
inline constexpr std::uint8_t kSidStartGame3     = 0x1A;  // CLIENT_STARTGAME3 / SERVER_STARTGAME3_ACK
inline constexpr std::uint8_t kSidCloseGame2     = 0x1F;  // CLIENT_CLOSEGAME2 (empty body)
inline constexpr std::uint8_t kSidJoinGame       = 0x22;  // CLIENT_JOIN_GAME
inline constexpr std::uint8_t kSidGameReport     = 0x2C;  // CLIENT_GAME_REPORT

// Misc / anti-cheat / advisory SIDs.
inline constexpr std::uint8_t kSidReadMemory     = 0x17;  // SERVER_READMEMORY / CLIENT_READMEMORY (anti-cheat)
inline constexpr std::uint8_t kSidUnknown1B      = 0x1B;  // CLIENT_UNKNOWN_1B (game IP/port advisory)
inline constexpr std::uint8_t kSidUnknown24      = 0x24;  // CLIENT_UNKNOWN_24 (empty body)
inline constexpr std::uint8_t kSidMapAuth1       = 0x32;  // CLIENT_MAPAUTHREQ1 / SERVER_MAPAUTHREPLY1
inline constexpr std::uint8_t kSidMapAuth2       = 0x3C;  // CLIENT_MAPAUTHREQ2 / SERVER_MAPAUTHREPLY2
inline constexpr std::uint8_t kSidChangeClient   = 0x5C;  // CLIENT_CHANGECLIENT

/// SID_NULL — keepalive, no payload.

struct Null {
    constexpr bool operator==(const Null&) const = default;
};

/// SID_PING — both directions carry a single u32 cookie ("ticks").
/// Server sends an ECHOREQ, client mirrors it back as ECHOREPLY.
struct Ping {
    std::uint32_t ticks = 0;
    constexpr bool operator==(const Ping&) const = default;
};

/// SID_AUTH_INFO (client → server) — first message after the init byte.

}  // namespace pvpgn::protocol::bnet
