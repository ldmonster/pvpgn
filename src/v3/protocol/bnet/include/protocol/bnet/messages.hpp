// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages.hpp
/// Battle.net binary protocol messages — pure value types, no logic.
///
/// Each message corresponds to a `SID_*` packet code. The wire format is
/// documented inline (matches `src/common/bnet_protocol.h` from the
/// legacy tree byte-for-byte). New SIDs are added by:
///   1. Appending a struct here.
///   2. Adding an arm to `ClientMessage` / `ServerMessage`.
///   3. Implementing `decode_*` / `encode_*` in `codec.cpp`.
///
/// **No dependency on domain types** — `std::string`, integers,
/// `std::vector<std::byte>`. Conversions to/from `AccountId` etc. happen
/// in the FSM, not here.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

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
struct AuthInfo {
    std::uint32_t protocol_id  = 0;
    std::uint32_t platform_id  = 0;        ///< 'IX86', 'PMAC', ...
    std::uint32_t game_id      = 0;        ///< 'SEXP', 'W3XP', ...
    std::uint32_t version_id   = 0;
    std::uint32_t language_id  = 0;
    std::uint32_t local_ip     = 0;
    std::uint32_t tz_bias      = 0;
    std::uint32_t mpq_locale   = 0;
    std::uint32_t lang_id      = 0;
    std::string   country_abbr;
    std::string   country;
    bool operator==(const AuthInfo&) const = default;
};

/// SID_AUTH_INFO (server → client) — legacy `SERVER_AUTHREQ_109` (0x50).
/// Carries the version-check seed and MPQ filename, server token used
/// for the password hash, and the W3 logon-type flag.
struct AuthInfoReply {
    std::uint32_t logontype    = 0;   ///< 0 = std, 2 = W3/W3XP (NLS).
    std::uint32_t server_token = 0;   ///< aka sessionkey.
    std::uint32_t session_num  = 0;
    std::uint64_t timestamp    = 0;   ///< Windows FILETIME.
    std::string   mpq_filename;       ///< e.g. "ver-IX86-1.mpq"
    std::string   checksum_formula;   ///< version-check equation
    bool operator==(const AuthInfoReply&) const = default;
};

/// SID_AUTH_CHECK (server → client) reply.
/// `result` codes: 0 = passed, others fail with a reason string.
struct AuthCheckReply {
    std::uint32_t result = 0;
    std::string   info;       ///< MPQ name / reason / "" on success.
    bool operator==(const AuthCheckReply&) const = default;
};

/// SID_AUTH_CHECK (client → server) — version + CD-key proof.
/// Matches legacy `CLIENT_AUTHREQ_109` (0x51). The cdkey payloads are
/// kept opaque so the codec doesn't impose a parse on cdkey bytes; the
/// FSM/auth service decodes them.
struct CdKeyInfo {
    std::uint32_t                public_value = 0;  // legacy "len"
    std::uint32_t                product      = 0;
    std::uint32_t                checksum     = 0;
    std::uint32_t                unknown      = 0;
    std::array<std::uint32_t, 5> hash{};
    bool operator==(const CdKeyInfo&) const = default;
};

struct AuthCheckRequest {
    std::uint32_t          ticks       = 0;
    std::uint32_t          gameversion = 0;
    std::uint32_t          checksum    = 0;
    std::uint32_t          spawn       = 0;  ///< 1 = spawn copy
    std::vector<CdKeyInfo> cdkeys;            ///< 1 (D2) or 2 (LoD)
    std::string            exe_info;          ///< e.g. "Game.exe 09/15/03 ..."
    std::string            cdkey_owner;
    bool operator==(const AuthCheckRequest&) const = default;
};

/// SID_LOGONRESPONSE2 (client → server).
struct LogonResponse2 {
    std::uint32_t              client_token = 0;
    std::uint32_t              server_token = 0;
    std::array<std::uint32_t,5> password_hash{};  ///< 5×u32 SHA-1
    std::string                username;
    bool operator==(const LogonResponse2&) const = default;
};

/// SID_LOGONRESPONSE2 (server → client). Result codes from legacy:
///   0x00 success, 0x01 account doesn't exist, 0x02 invalid password,
///   0x06 account closed (reason follows).
struct LogonResponse2Reply {
    std::uint32_t result = 0;
    std::string   reason;     ///< only present when result == 0x06.
    bool operator==(const LogonResponse2Reply&) const = default;
};

/// SID_JOINCHANNEL (client → server).
/// `flags`: 0 normal, 1 first-join, 2 forced-join.
struct JoinChannel {
    std::uint32_t flags = 0;
    std::string   channel;
    bool operator==(const JoinChannel&) const = default;
};

/// SID_ENTERCHAT (client → server).
struct EnterChatRequest {
    std::string username;     ///< empty for product default
    std::string statstring;
    bool operator==(const EnterChatRequest&) const = default;
};

/// SID_ENTERCHAT (server → client).
struct EnterChatReply {
    std::string unique_name;
    std::string statstring;
    std::string account;
    bool operator==(const EnterChatReply&) const = default;
};

/// SID_CHATCOMMAND (client → server).
struct ChatCommand {
    std::string text;
    bool operator==(const ChatCommand&) const = default;
};

/// SID_CHATEVENT (server → client). Event IDs match legacy `EID_*`.
struct ChatEvent {
    std::uint32_t event_id     = 0;
    std::uint32_t flags        = 0;
    std::uint32_t ping_ms      = 0;
    std::uint32_t user_ip      = 0;
    std::uint32_t acct_number  = 0;
    std::uint32_t registration = 0;
    std::string   username;
    std::string   text;
    bool operator==(const ChatEvent&) const = default;
};

// --- SID_GETADVLISTEX (0x09) — public game list ----------------------------

/// SID_GETADVLISTEX (client → server).
/// `unknown1/2/3` track legacy fields; opaque to the codec.
struct GameListRequest {
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
    std::uint32_t unknown3 = 0;
    std::uint32_t max_games = 0;
    std::string   game_name;   ///< empty = "any game"
    bool operator==(const GameListRequest&) const = default;
};

/// Entry inside a SERVER_GAMELISTREPLY. Ports + IPs are kept in their
/// wire byte order (big-endian on the wire); the codec stores them as
/// host integers reconstructed from the BE bytes.
struct GameListEntry {
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint16_t unknown3 = 0;
    std::uint16_t port     = 0;   ///< host order (wire was BE)
    std::uint32_t game_ip  = 0;   ///< host order (wire was BE)
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::uint32_t status   = 0;
    std::uint32_t unknown6 = 0;
    std::string   game_name;
    std::string   password;       ///< clear-text legacy field; empty if none
    std::string   info;
    bool operator==(const GameListEntry&) const = default;
};

/// SID_GETADVLISTEX (server → client).
/// When `sstatus` ≠ 0 the entries vector is empty and the status code
/// carries the per-request error.
struct GameListReply {
    std::uint32_t                sstatus = 0;
    std::vector<GameListEntry>   entries;
    bool operator==(const GameListReply&) const = default;
};

// --- SID_LADDERSEARCH (0x2F) — find player on ladder -----------------------

struct LadderSearchRequest {
    std::uint32_t client_tag = 0;   ///< e.g. 'SEXP', 'W3XP'
    std::uint32_t id         = 0;   ///< 1 standard, 3 ironman
    std::uint32_t type       = 0;   ///< 0 rated, 2 wins, 3 games
    std::string   player_name;
    bool operator==(const LadderSearchRequest&) const = default;
};

struct LadderSearchReply {
    std::uint32_t rank = 0xFFFFFFFFu;   ///< 0 = first; 0xFFFFFFFF = none
    bool operator==(const LadderSearchReply&) const = default;
};

// --- SID_GETFILETIME (0x33) — file-transfer init ---------------------------

struct FileInfoRequest {
    std::uint32_t type     = 0;   ///< TOS=0x1A, gateways=0x1B, ...
    std::uint32_t unknown2 = 0;   ///< always zero on the wire
    std::string   filename;
    bool operator==(const FileInfoRequest&) const = default;
};

struct FileInfoReply {
    std::uint32_t type      = 0;
    std::uint32_t unknown2  = 0;
    std::uint64_t timestamp = 0;  ///< Windows FILETIME (100-ns since 1601)
    std::string   filename;
    bool operator==(const FileInfoReply&) const = default;
};

// --- SID_CDKEY2 (0x36) — second-key (LoD/SC) proof -------------------------

/// SID_CDKEY2 (client → server). Used by Diablo II / Starcraft to
/// register a *second* CD-key after the version-check passes.
struct CdKey2Request {
    std::uint32_t                spawn        = 0;  ///< 1 = spawn copy
    std::uint32_t                keylen       = 0;  ///< excluding NUL
    std::uint32_t                product_id   = 0;
    std::uint32_t                key_value    = 0;  ///< public part
    std::uint32_t                server_token = 0;  ///< session key
    std::uint32_t                ticks        = 0;
    std::array<std::uint32_t, 5> key_hash{};
    std::string                  owner;
    bool operator==(const CdKey2Request&) const = default;
};

/// SID_CDKEY2 (server → client) — `result` is one of:
/// 1 OK, 2 invalid, 3 wrong product, 4 banned, 5 in use.
struct CdKey2Reply {
    std::uint32_t result = 0;
    std::string   owner;       ///< server may echo the owner string on result=5
    bool operator==(const CdKey2Reply&) const = default;
};

// --- SID_FRIENDSLIST (0x65) — friends overview -----------------------------

/// SID_FRIENDSLIST request — empty body.
struct FriendsListRequest {
    bool operator==(const FriendsListRequest&) const = default;
};

/// Single entry inside a SERVER_FRIENDSLISTREPLY.
struct FriendsListEntry {
    std::string   name;           ///< friend account name
    std::uint8_t  status   = 0;   ///< FRIEND_TYPE_* bitfield
    std::uint8_t  location = 0;   ///< FRIENDSTATUS_*
    std::uint32_t client_tag = 0; ///< e.g. 'W3XP'
    std::string   location_name;  ///< channel/game name; "" if offline
    bool operator==(const FriendsListEntry&) const = default;
};

struct FriendsListReply {
    std::vector<FriendsListEntry> entries;
    bool operator==(const FriendsListReply&) const = default;
};

// --- SID_FRIENDINFO (0x66) — single-friend update --------------------------

struct FriendInfoRequest {
    std::uint8_t friend_num = 0;
    bool operator==(const FriendInfoRequest&) const = default;
};

struct FriendInfoReply {
    std::uint8_t  friend_num = 0;
    std::uint8_t  type       = 0;     ///< FRIEND_TYPE_*
    std::uint8_t  status     = 0;     ///< location code
    std::uint32_t client_tag = 0;
    std::string   game_name;          ///< empty if not in game
    bool operator==(const FriendInfoReply&) const = default;
};

// --- SID_FRIENDADD (0x67) / DEL (0x68) / MOVE (0x69) — server-only acks ----

/// 0x67 server → client. Wire layout matches `FriendsListEntry` (name, then
/// FRIEND_TYPE_* status byte, FRIENDSTATUS_* location byte, clienttag u32,
/// then trailing game/channel name cstring).
struct FriendAddAck {
    std::string   name;
    std::uint8_t  status     = 0;
    std::uint8_t  location   = 0;
    std::uint32_t client_tag = 0;
    std::string   location_name;
    bool operator==(const FriendAddAck&) const = default;
};

/// 0x68 server → client. Single byte: the slot index of the removed friend.
struct FriendDelAck {
    std::uint8_t friend_num = 0;
    bool operator==(const FriendDelAck&) const = default;
};

/// 0x69 server → client. Two slot indices being swapped in the roster.
struct FriendMoveAck {
    std::uint8_t pos1 = 0;
    std::uint8_t pos2 = 0;
    bool operator==(const FriendMoveAck&) const = default;
};

// --- SID_ARRANGEDTEAM_* (0x60..0x63, 0xFD) — W3 arranged-team handshake ---

/// 0x60 client → server: open the arranged-team friend screen. Empty body.
struct ArrangedTeamFriendScreenRequest {
    bool operator==(const ArrangedTeamFriendScreenRequest&) const = default;
};

/// 0x60 server → client: list of names eligible for arranged-team invites.
/// `f_count` byte prefixes a sequence of name cstrings.
struct ArrangedTeamFriendScreenReply {
    std::vector<std::string> names;
    bool operator==(const ArrangedTeamFriendScreenReply&) const = default;
};

/// 0x61 client → server: invite a set of friends into an arranged team.
/// `count` is the client's request cookie, `id` the client-supplied team
/// id, `unknown1` is always 0x00000001 on the wire.
struct ArrangedTeamInviteFriendRequest {
    std::uint32_t            count    = 0;
    std::uint32_t            id       = 0;
    std::uint32_t            unknown1 = 1;
    std::vector<std::string> friends;
    bool operator==(const ArrangedTeamInviteFriendRequest&) const = default;
};

/// 0x61 server → client: ack the invite request. `info[5]` is opaque
/// arranged-team metadata returned by bnetd.
struct ArrangedTeamInviteFriendAck {
    std::uint32_t                count     = 0;
    std::uint32_t                id        = 0;
    std::uint32_t                timestamp = 0;
    std::uint8_t                 team_size = 0;
    std::array<std::uint32_t, 5> info{};
    bool operator==(const ArrangedTeamInviteFriendAck&) const = default;
};

/// 0x62 server → client: notify the team that one invitee declined.
struct ArrangedTeamMemberDecline {
    std::uint32_t count       = 0;
    std::uint32_t action      = 0;
    std::string   decliner_name;
    bool operator==(const ArrangedTeamMemberDecline&) const = default;
};

/// 0x63 server → client: deliver an arranged-team invite to the invitee.
/// `inviter_ip` is a raw u32 in network order on the wire; we keep it as the
/// untouched u32 so the codec stays format-faithful. `port` is u16 LE.
struct ArrangedTeamSendInvite {
    std::uint32_t            count       = 0;
    std::uint32_t            id          = 0;
    std::uint32_t            inviter_ip  = 0;
    std::uint16_t            port        = 0;
    std::string              inviter_name;
    std::vector<std::string> other_names;
    bool operator==(const ArrangedTeamSendInvite&) const = default;
};

/// 0x63 client → server: invitee's reply. `option` is 3 (accept) or 2 (decline).
struct ArrangedTeamAcceptDeclineInvite {
    std::uint32_t count        = 0;
    std::uint32_t id           = 0;
    std::uint32_t option       = 0;
    std::string   inviter_name;
    bool operator==(const ArrangedTeamAcceptDeclineInvite&) const = default;
};

/// 0xFD client → server: invitee opened the invite dialog. Empty body.
struct ArrangedTeamAcceptInvite {
    bool operator==(const ArrangedTeamAcceptInvite&) const = default;
};

// --- SID_STARTADVEX3 (0x1C) — host game -----------------------------------

/// 0x1C client → server: open or update a hosted game advertisement.
struct StartGame4Request {
    std::uint16_t status   = 0;
    std::uint16_t flag     = 0;
    std::uint32_t unknown2 = 0;
    std::uint16_t gametype = 0;
    std::uint16_t option   = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::string   game_name;
    std::string   password;
    std::string   info;
    bool operator==(const StartGame4Request&) const = default;
};

/// 0x1C server → client. `reply` = 0 on success.
struct StartGame4Ack {
    std::uint32_t reply = 0;
    bool operator==(const StartGame4Ack&) const = default;
};

// --- SID_UDPPINGRESPONSE (0x14) — UDP echo confirmation -------------------

/// 0x14 client → server. Sent by the client to confirm a successful UDP
/// echo round-trip; the four-byte echo equals what the server sent (usually
/// the ASCII tag `"tenb"`).
struct UdpOk {
    std::uint32_t echo = 0;
    bool operator==(const UdpOk&) const = default;
};

// --- SID_GETLADDERDATA (0x2E) — paginated ladder snapshot -----------------

/// 0x2E client → server: request a slice of a ladder.
struct LadderListRequest {
    std::uint32_t client_tag = 0;
    std::uint32_t id         = 0;   ///< 1 standard, 3 ironman
    std::uint32_t type       = 0;   ///< 0 highest-rated, 2 most-wins, 3 most-games
    std::uint32_t start      = 0;   ///< first row in the snapshot
    std::uint32_t count      = 0;   ///< number of rows requested
    bool operator==(const LadderListRequest&) const = default;
};

/// A `current` or `active` season block inside `LadderListEntry`.
struct LadderDataBlock {
    std::uint32_t wins        = 0;
    std::uint32_t loss        = 0;
    std::uint32_t disconnect  = 0;
    std::uint32_t rating      = 0;
    std::uint32_t rank        = 0;
    bool operator==(const LadderDataBlock&) const = default;
};

/// One row inside SERVER_LADDERREPLY.
/// `ttest[]` is treated as opaque six-u32 padding; clients are expected to
/// ignore the contents, but we preserve them so the encoder reproduces the
/// original wire exactly.
struct LadderListEntry {
    LadderDataBlock              current;
    LadderDataBlock              active;
    std::array<std::uint32_t, 6> ttest{};
    std::uint64_t                lastgame_current = 0;
    std::uint64_t                lastgame_active  = 0;
    std::string                  player_name;
    bool operator==(const LadderListEntry&) const = default;
};

/// 0x2E server → client. The header fields echo the request; `count` is the
/// number of entries that follow.
struct LadderListReply {
    std::uint32_t                 client_tag = 0;
    std::uint32_t                 id         = 0;
    std::uint32_t                 type       = 0;
    std::uint32_t                 start      = 0;
    std::uint32_t                 count      = 0;
    std::vector<LadderListEntry>  entries;
    bool operator==(const LadderListReply&) const = default;
};

// --- SID_CHECKAD (0x15) — banner request/reply ---------------------------

/// 0x15 client → server. `prev_adid` is the previously-displayed banner
/// (zero on the first request). `ticks` is a Unix-style timestamp.
struct AdRequest {
    std::uint32_t arch_tag   = 0;
    std::uint32_t client_tag = 0;
    std::uint32_t prev_adid  = 0;
    std::uint32_t ticks      = 0;
    bool operator==(const AdRequest&) const = default;
};

/// 0x15 server → client. `filename` is the local ad asset to display and
/// `link` is the click-through URL. `extension_tag` advertises the file
/// kind in a "forward" (non-reversed) FOURCC layout.
struct AdReply {
    std::uint32_t adid           = 0;
    std::uint32_t extension_tag  = 0;
    std::uint64_t timestamp      = 0;
    std::string   filename;
    std::string   link;
    bool operator==(const AdReply&) const = default;
};

// --- CLIENT_ADCLICK (0x16) — banner click notification -------------------

struct AdClick {
    std::uint32_t adid     = 0;
    std::uint32_t unknown1 = 0;
    bool operator==(const AdClick&) const = default;
};

// --- CLIENT_ADACK (0x21) — banner display acknowledgement -----------------

/// Sent after the client successfully displayed the banner returned in
/// SERVER_ADREPLY.
struct AdAck {
    std::uint32_t arch_tag   = 0;
    std::uint32_t client_tag = 0;
    std::uint32_t adid       = 0;
    std::string   adfile;
    std::string   adlink;
    bool operator==(const AdAck&) const = default;
};

// --- CLIENT_ADCLICK2 / SERVER_ADCLICKREPLY2 (0x41) ------------------------

/// 0x41 client → server. Click on a Diablo-II era banner.
struct AdClick2Request {
    std::uint32_t adid = 0;
    bool operator==(const AdClick2Request&) const = default;
};

/// 0x41 server → client. Returns the URL to open in the user's browser.
struct AdClick2Reply {
    std::uint32_t adid = 0;
    std::string   link;
    bool operator==(const AdClick2Reply&) const = default;
};

// --- SID_NEWS_INFO / MOTD (0x46) ------------------------------------------

/// 0x46 client → server. The client asks the server for news/MOTD items
/// newer than `last_news_time` (a Unix-style timestamp).
struct MotdRequest {
    std::uint32_t last_news_time = 0;
    bool operator==(const MotdRequest&) const = default;
};

/// 0x46 server → client. One news entry. The body holds five timestamps
/// plus a text payload; `text` is the displayable message.
struct MotdReply {
    std::uint8_t  msg_type        = 1;  ///< 1 = news entry (only value observed)
    std::uint32_t curr_time       = 0;  ///< server-side wall clock
    std::uint32_t first_news_time = 0;  ///< oldest news item's timestamp
    std::uint32_t timestamp       = 0;  ///< this news item's timestamp
    std::uint32_t timestamp2      = 0;  ///< right-panel marker timestamp
    std::string   text;
    bool operator==(const MotdReply&) const = default;
};

// --- CLIENT_PROGIDENT2 / SERVER_CHANNELLIST (0x0B) ------------------------

/// 0x0B client → server. The client announces its product to receive a
/// channel listing.
struct ChannelListRequest {
    std::uint32_t client_tag = 0;
    bool operator==(const ChannelListRequest&) const = default;
};

/// 0x0B server → client. List of channel names, terminated on the wire by
/// an empty cstring (so an "empty list" still consumes one NUL byte).
struct ChannelListReply {
    std::vector<std::string> channels;
    bool operator==(const ChannelListReply&) const = default;
};

// --- CLIENT_LEAVECHANNEL (0x10) ------------------------------------------

/// 0x10 client → server. Empty body.
struct LeaveChannel {
    bool operator==(const LeaveChannel&) const = default;
};

// --- SERVER_REGSNOOPREQ / CLIENT_REGSNOOPREPLY (0x18) --------------------

/// 0x18 server → client. The server asks the client to look up a registry
/// value. Used historically by Battle.net for telemetry / spyware-style
/// data collection.
struct RegSnoopRequest {
    std::uint32_t unknown1  = 0;
    std::uint32_t hkey      = 0;
    std::string   reg_key;
    std::string   value_name;
    bool operator==(const RegSnoopRequest&) const = default;
};

/// 0x18 client → server. If the registry value exists, the client returns
/// it as a raw opaque payload (string, dword, or binary blob).
struct RegSnoopReply {
    std::uint32_t              unknown1 = 0;
    std::vector<std::byte>     value;
    bool operator==(const RegSnoopReply&) const = default;
};

// --- SID_PROFILE (0x35) — short user-profile lookup ----------------------

/// 0x35 client → server. `cookie` round-trips so the client can correlate
/// asynchronous replies.
struct ProfileRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    bool operator==(const ProfileRequest&) const = default;
};

/// 0x35 server → client. `fail` is non-zero when the player has no profile
/// (or is offline); the description/location/clan_tag trailer is only
/// present on success.
struct ProfileReply {
    std::uint32_t cookie      = 0;
    std::uint8_t  fail        = 0;
    std::string   description;
    std::string   location;
    std::uint32_t clan_tag    = 0;
    bool operator==(const ProfileReply&) const = default;
};

// --- SID_SETEMAIL (0x59) — server-initiated email prompt -----------------

/// 0x59 server → client. Empty body. Sent before the login-ok packet to
/// prompt the client to display an email-input screen.
struct SetEmailRequest {
    bool operator==(const SetEmailRequest&) const = default;
};

/// 0x59 client → server. Returns the email the user typed.
struct SetEmailReply {
    std::string email;
    bool operator==(const SetEmailReply&) const = default;
};

// --- SID_ICONREQ (0x2D) — icons.bni metadata ----------------------------

/// 0x2D client → server. Empty body — the client asks for the icons.bni
/// filename + timestamp so it can decide whether to download it.
struct IconRequest {
    bool operator==(const IconRequest&) const = default;
};

/// 0x2D server → client. `timestamp` is the file modification time (a
/// Windows FILETIME-style u64).
struct IconReply {
    std::uint64_t timestamp = 0;
    std::string   filename;
    bool operator==(const IconReply&) const = default;
};

// --- SID_GETPASSWORD (0x5A) — password-recovery request ------------------

/// 0x5A client → server. The user asked to recover the password for an
/// account; the server is expected to email the password (or a reset
/// token) to the supplied address if it matches the on-record value.
struct GetPasswordRequest {
    std::string account_name;
    std::string email;
    bool operator==(const GetPasswordRequest&) const = default;
};

// --- SID_CHANGEEMAIL (0x5B) — email-change request ----------------------

/// 0x5B client → server. The user wants to swap the email tied to an
/// account. The server is expected to verify `old_email` against its
/// records before storing `new_email`.
struct ChangeEmailRequest {
    std::string account_name;
    std::string old_email;
    std::string new_email;
    bool operator==(const ChangeEmailRequest&) const = default;
};

// --- SID_CRASHDUMP (0x5D) — client crash report --------------------------

/// 0x5D client → server. The body is an opaque blob; the original format
/// notes describe it as containing the client version, exception code and
/// code address but the wire layout is not stable across builds. We
/// preserve it as raw bytes.
struct CrashDump {
    std::vector<std::byte> data;
    bool operator==(const CrashDump&) const = default;
};

// --- SID_UNKNOWN_37 (0x37) — legacy D2 character list ---------------------
//
// Client→server announces how many "open" characters live on the user's
// machine, optionally followed by an opaque blob of d2char_info structs.
// Server→client answers with two reserved u32 fields, a u32 character
// count, and a trailing opaque blob of per-character records (each record
// is a realm,charname C-string followed by binary stat bytes). We preserve
// the tail verbatim as `std::vector<std::byte>` — the legacy code never
// inspects individual fields once the count is known.
struct CharListRequest {
    std::uint32_t          open_count = 0;
    std::vector<std::byte> char_data;
    bool operator==(const CharListRequest&) const = default;
};

struct CharListReply {
    std::uint32_t          unknown1   = 0;
    std::uint32_t          max_chars  = 8;
    std::uint32_t          count      = 0;
    std::vector<std::byte> char_data;
    bool operator==(const CharListReply&) const = default;
};

// --- SID_SERVERLIST (0x04) — alt-server fallback list ---------------------
//
// Server→client only. The legacy code never sends this in practice but
// the format is well-documented: one reserved u32 followed by a single
// C-string containing semicolon-delimited host names / dotted IPs that
// the client should add to its registry as alternate Battle.net servers.
struct ServerList {
    std::uint32_t unknown1 = 0;
    std::string   servers;  // "209.67.136.174;207.69.194.210;..."
    bool operator==(const ServerList&) const = default;
};

// --- SID_MESSAGEBOX (0x19) — server-pushed modal dialog ------------------
//
// Server→client only. Tells the official client to display a Windows
// MessageBox with the supplied caption and text. `style` mirrors the
// Win32 `MB_*` flag bits (OK / OKCANCEL / YESNO at a minimum).
struct MessageBox {
    std::uint32_t style   = 0;  // SERVER_MESSAGEBOX_OK / _OKCANCEL / _YESNO
    std::string   text;
    std::string   caption;
    bool operator==(const MessageBox&) const = default;
};

inline constexpr std::uint32_t kMessageBoxStyleOk        = 0x00000000;
inline constexpr std::uint32_t kMessageBoxStyleOkCancel  = 0x00000001;
inline constexpr std::uint32_t kMessageBoxStyleYesNo     = 0x00000004;

// --- SID_REALMLIST_110 (0x40) — realm selection list --------------------
//
// 1.10+ realm-list exchange. Client request has no body; server reply
// carries a reserved u32, a u32 entry count, and `count` repetitions of
// `{ u32 unknown, cstring name, cstring description }`.
struct RealmListRequest {
    bool operator==(const RealmListRequest&) const = default;
};

struct RealmListEntry {
    std::uint32_t unknown    = 1;
    std::string   name;
    std::string   description;
    bool operator==(const RealmListEntry&) const = default;
};

struct RealmListReply {
    std::uint32_t                unknown1 = 0;
    std::vector<RealmListEntry>  entries;
    bool operator==(const RealmListReply&) const = default;
};

// --- SID_REALMJOIN_109 (0x3E) — realm session handshake -----------------
//
// Client: 32-bit `seqno` cookie, a 20-byte `seqno_hash` (5 u32 words),
// and the chosen realm name as a C-string.
// Server: large fixed header (see legacy `t_server_realmjoinreply_109`),
// 20-byte `secret_hash`, and the trailing account-name C-string. The
// `port` field is the only big-endian member (network byte order).
struct RealmJoinRequest {
    std::uint32_t                seqno = 0;
    std::array<std::uint32_t, 5> seqno_hash{};
    std::string                  realm_name;
    bool operator==(const RealmJoinRequest&) const = default;
};

struct RealmJoinReply {
    std::uint32_t                seqno        = 0;
    std::uint32_t                u1           = 0;
    std::uint32_t                bncs_addr1   = 0;
    std::uint32_t                session_num  = 0;
    std::uint32_t                addr         = 0;
    std::uint16_t                port         = 0;   // big-endian on wire
    std::uint16_t                u3           = 0;
    std::uint32_t                session_key  = 0;
    std::uint32_t                u5           = 0;
    std::uint32_t                u6           = 0;
    std::uint32_t                client_tag   = 0;
    std::uint32_t                version_id   = 0;
    std::uint32_t                bncs_addr2   = 0;
    std::uint32_t                u7           = 0;
    std::array<std::uint32_t, 5> secret_hash{};
    std::string                  account_name;
    bool operator==(const RealmJoinReply&) const = default;
};

// --- SID_WARCRAFTGENERAL (0x44) — anongame sub-option multiplexer ------
//
// The 0x44 packet space is a small switch keyed on the first byte
// (`sub_option`) of the payload. Each option (SEARCH / INFOS / CANCEL /
// PROFILE / AT_SEARCH / AT_INVITER_SEARCH / TOURNAMENT / PROFILE_CLAN /
// GET_ICON / SET_ICON on the client side; SEARCH / FOUND / CANCEL on the
// server side) has its own internal layout described in the legacy
// `anongame_protocol.h`. The strangler bridge transports these packets
// verbatim so the application layer can route on `sub_option`; per-option
// parsers can be added incrementally without redefining the wire envelope.
struct WarcraftGeneralRequest {
    std::uint8_t           sub_option = 0;
    std::vector<std::byte> data;
    bool operator==(const WarcraftGeneralRequest&) const = default;
};

struct WarcraftGeneralReply {
    std::uint8_t           sub_option = 0;
    std::vector<std::byte> data;
    bool operator==(const WarcraftGeneralReply&) const = default;
};

// Sub-option codes from anongame_protocol.h — exported for application use.
inline constexpr std::uint8_t kAnonGameClientSearch          = 0x00;
inline constexpr std::uint8_t kAnonGameClientInfos           = 0x02;
inline constexpr std::uint8_t kAnonGameClientCancel          = 0x03;
inline constexpr std::uint8_t kAnonGameClientProfile         = 0x04;
inline constexpr std::uint8_t kAnonGameClientAtSearch        = 0x05;
inline constexpr std::uint8_t kAnonGameClientAtInviterSearch = 0x06;
inline constexpr std::uint8_t kAnonGameClientTournament      = 0x07;
inline constexpr std::uint8_t kAnonGameClientProfileClan     = 0x08;
inline constexpr std::uint8_t kAnonGameClientGetIcon         = 0x09;
inline constexpr std::uint8_t kAnonGameClientSetIcon         = 0x0A;
inline constexpr std::uint8_t kAnonGameServerSearch          = 0x00;
inline constexpr std::uint8_t kAnonGameServerFound           = 0x01;
inline constexpr std::uint8_t kAnonGameServerCancel          = 0x03;

// --- SID_REQUIREDWORK (0x4C) / SID_EXTRAWORK (0x4B) ---------------------
//
// Anti-cheat work-factor exchange used by W3 / D2 “extra work” MPQs.
// Server→client (`RequiredWork`) names the file the client must hash and
// optionally execute. Client→server (`ExtraWork`) returns the computed
// blob keyed by `gametype` plus a 16-bit length prefix and opaque data.
struct RequiredWork {
    std::string filename;
    bool operator==(const RequiredWork&) const = default;
};

struct ExtraWork {
    std::uint16_t          game_type = 0;
    std::vector<std::byte> data;  // `length` is the size of this blob on the wire
    bool operator==(const ExtraWork&) const = default;
};

// --- SID_REALMLIST (0x34) — pre-1.10 realm exchange --------------------
//
// Client carries two u32 cookies (always zero in practice). Server reply
// carries a reserved u32 plus a u32 entry count followed by `count`
// `RealmListLegacyEntry` records: seven u32 fixed fields (the legacy
// `t_server_realmlistreply_data` shape, layout documented in
// `bnet_protocol.h`) followed by realm name and description C-strings.
struct RealmListLegacyRequest {
    std::uint32_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
    bool operator==(const RealmListLegacyRequest&) const = default;
};

struct RealmListLegacyEntry {
    std::uint32_t unknown3 = 0xC0000000u;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::uint32_t unknown6 = 0;
    std::uint32_t unknown7 = 0x00018210u;
    std::uint32_t unknown8 = 0xFFFFFFFFu;
    std::uint32_t unknown9 = 0;
    std::string   name;
    std::string   description;
    bool operator==(const RealmListLegacyEntry&) const = default;
};

struct RealmListLegacyReply {
    std::uint32_t                     unknown1 = 0;
    std::vector<RealmListLegacyEntry> entries;
    bool operator==(const RealmListLegacyReply&) const = default;
};

// --- SID_CDKEY3 (0x42) — multi-CD-key authentication ------------------
//
// 1.13-era replacement for SID_CDKEY2. The client request carries a salt
// (`unknown1`) plus six u32 fixed cookies, a 20-byte `key_hash` (5 u32
// words), and the owner name as a C-string. Default cookie values match
// the `CLIENT_CDKEY3_UNKNOWN*` constants captured in the legacy tree.
// The server reply is a single u32 `message` (0 == OK) followed by an
// optional owner-name C-string echo.
struct CdKey3Request {
    std::uint32_t                unknown1 = 0xFFFFFFFFu;
    std::uint32_t                unknown2 = 0x00000001u;
    std::uint32_t                unknown3 = 0x00000000u;
    std::uint32_t                unknown4 = 0x00000010u;
    std::uint32_t                unknown5 = 0x00000006u;
    std::uint32_t                unknown6 = 0x00123456u;
    std::uint32_t                unknown7 = 0x00000000u;
    std::array<std::uint32_t, 5> key_hash{};
    std::string                  owner_name;
    bool operator==(const CdKey3Request&) const = default;
};

inline constexpr std::uint32_t kCdKeyReply3MessageOk = 0x00000000u;

struct CdKey3Reply {
    std::uint32_t message = kCdKeyReply3MessageOk;
    std::string   owner_name;
    bool operator==(const CdKey3Reply&) const = default;
};

// --- SID_CREATEACCOUNT2 / CREATEACCOUNT_W3 (0x52) -----------------------
//
// W3-era NLS account creation. Client sends a 32-byte SRP salt, a 32-byte
// password verifier, then the account name as a C-string. Server replies
// with a single u32 result code (0 == OK).
struct CreateAccount2Request {
    std::array<std::uint8_t, 32> salt{};
    std::array<std::uint8_t, 32> password_verifier{};
    std::string                  account_name;
    bool operator==(const CreateAccount2Request&) const = default;
};

inline constexpr std::uint32_t kCreateAccount2ResultOk          = 0x00000000u;
inline constexpr std::uint32_t kCreateAccount2ResultExists      = 0x00000004u;
inline constexpr std::uint32_t kCreateAccount2ResultEmpty       = 0x00000007u;
inline constexpr std::uint32_t kCreateAccount2ResultInvalid     = 0x00000008u;
inline constexpr std::uint32_t kCreateAccount2ResultBanned      = 0x00000009u;
inline constexpr std::uint32_t kCreateAccount2ResultShort       = 0x0000000Au;
inline constexpr std::uint32_t kCreateAccount2ResultPunctuation = 0x0000000Bu;
inline constexpr std::uint32_t kCreateAccount2ResultPunctuation2 = 0x0000000Cu;

struct CreateAccount2Reply {
    std::uint32_t result = kCreateAccount2ResultOk;
    bool operator==(const CreateAccount2Reply&) const = default;
};

// --- SID_LOGINREQ_W3 / SID_LOGINREPLY_W3 (0x53) --------------------------
//
// NLS step A. Client → server: 32-byte SRP public key (`A`) followed by the
// account name as a C-string. Server → client: u32 status code, 32-byte
// `salt`, 32-byte server public key (`B`).
struct LoginW3Request {
    std::array<std::uint8_t, 32> client_public_key{};
    std::string                  account_name;
    bool operator==(const LoginW3Request&) const = default;
};

inline constexpr std::uint32_t kLoginW3MessageSuccess = 0x00000000u;
inline constexpr std::uint32_t kLoginW3MessageFailure = 0x00000001u;  // bad account / already on

struct LoginW3Reply {
    std::uint32_t                message = kLoginW3MessageSuccess;
    std::array<std::uint8_t, 32> salt{};
    std::array<std::uint8_t, 32> server_public_key{};
    bool operator==(const LoginW3Reply&) const = default;
};

// --- SID_LOGONPROOFREQ / SID_LOGONPROOFREPLY (0x54) ----------------------
//
// NLS step B. Client → server: 20-byte SRP M1 (`client_password_proof`).
// Server → client: u32 response code, 20-byte SRP M2
// (`server_password_proof`), and — when `response == CUSTOM` — a trailing
// C-string carrying the human-readable rejection reason.
struct LogonProofW3Request {
    std::array<std::uint8_t, 20> client_password_proof{};
    bool operator==(const LogonProofW3Request&) const = default;
};

inline constexpr std::uint32_t kLogonProofW3ResponseOk      = 0x00000000u;
inline constexpr std::uint32_t kLogonProofW3ResponseBadPass = 0x00000002u;
inline constexpr std::uint32_t kLogonProofW3ResponseEmail   = 0x0000000Eu;
inline constexpr std::uint32_t kLogonProofW3ResponseCustom  = 0x0000000Fu;

struct LogonProofW3Reply {
    std::uint32_t                response = kLogonProofW3ResponseOk;
    std::array<std::uint8_t, 20> server_password_proof{};
    std::string                  message;  ///< populated only for response == CUSTOM
    bool operator==(const LogonProofW3Reply&) const = default;
};

// --- SID_PASSCHANGEREQ / SID_PASSCHANGEREPLY (0x55) ---------------------
//
// NLS password-change step A. Layout mirrors LOGINREQ_W3 / LOGINREPLY_W3
// (0x53) byte-for-byte; the message id is the only difference. Client
// sends a 32-byte SRP `A` and the username; server returns a status
// code plus salt and `B`. ACCEPT=0, REJECT=1 (no such account).
struct PassChangeRequest {
    std::array<std::uint8_t, 32> client_public_key{};
    std::string                  account_name;
    bool operator==(const PassChangeRequest&) const = default;
};

inline constexpr std::uint32_t kPassChangeMessageAccept = 0x00000000u;
inline constexpr std::uint32_t kPassChangeMessageReject = 0x00000001u;

struct PassChangeReply {
    std::uint32_t                message = kPassChangeMessageAccept;
    std::array<std::uint8_t, 32> salt{};
    std::array<std::uint8_t, 32> server_public_key{};
    bool operator==(const PassChangeReply&) const = default;
};

// --- SID_PASSCHANGEPROOFREQ / SID_PASSCHANGEPROOFREPLY (0x56) -----------
//
// NLS password-change step B. Client sends an SRP M1 proof against the
// *old* password plus the new salt + new password verifier; server
// replies with a status and its M2 proof.
struct PassChangeProofRequest {
    std::array<std::uint8_t, 20> client_password_proof{};
    std::array<std::uint8_t, 32> salt{};
    std::array<std::uint8_t, 32> password_verifier{};
    bool operator==(const PassChangeProofRequest&) const = default;
};

inline constexpr std::uint32_t kPassChangeProofResponseOk      = 0x00000000u;
inline constexpr std::uint32_t kPassChangeProofResponseBadPass = 0x00000002u;

struct PassChangeProofReply {
    std::uint32_t                response = kPassChangeProofResponseOk;
    std::array<std::uint8_t, 20> server_password_proof{};
    bool operator==(const PassChangeProofReply&) const = default;
};

// --- SID_READUSERDATA (0x26) / SID_WRITEUSERDATA (0x27) -------------------
//
// Profile / record query API. Read: client picks a set of account names and
// a set of keys (`profile\sex`, `Record\Star\0\wins`, ...); server answers
// with `name_count * key_count` string values in row-major order. Write:
// client sends the same name×key matrix plus the new values to store. A
// `request_id` cookie round-trips on READ so the client can correlate
// asynchronous answers; WRITE has no cookie on the wire.

struct UserDataReadRequest {
    std::uint32_t            request_id = 0;
    std::vector<std::string> names;
    std::vector<std::string> keys;
    bool operator==(const UserDataReadRequest&) const = default;
};

struct UserDataReadReply {
    std::uint32_t            request_id = 0;
    std::uint32_t            name_count = 0;  ///< echoed from request
    std::uint32_t            key_count  = 0;  ///< echoed from request
    std::vector<std::string> values;          ///< size == name_count * key_count
    bool operator==(const UserDataReadReply&) const = default;
};

struct UserDataWriteRequest {
    std::vector<std::string> names;
    std::vector<std::string> keys;
    std::vector<std::string> values;          ///< size == names.size() * keys.size()
    bool operator==(const UserDataWriteRequest&) const = default;
};

// --- SID_CLAN_CREATE (0x70) family — clan administration -----------------
//
// Most clan management SIDs share a ``cookie`` (called ``count`` in the
// legacy code) which round-trips on the reply so the client can correlate
// outcomes with the issued request. The result codes for these replies
// match the ``CLAN_RESPONSE_*`` set documented in BnetDocs.

/// SID_CLAN_CREATE (0x70) — request: cookie + 4-byte clantag.
struct ClanCreateRequest {
    std::uint32_t cookie   = 0;
    std::uint32_t clan_tag = 0;       ///< 4-char clan tag, packed BE on wire (treated as u32 here)
    bool operator==(const ClanCreateRequest&) const = default;
};

/// SID_CLAN_CREATE (0x70) — reply: cookie + check_result + friend_count +
/// `friend_count` cstrings (members already-online and eligible to be invited).
struct ClanCreateReply {
    std::uint32_t            cookie       = 0;
    std::uint8_t             check_result = 0;
    std::vector<std::string> friend_names;
    bool operator==(const ClanCreateReply&) const = default;
};

/// SID_CLAN_DISBAND (0x73) — request: cookie only.
struct ClanDisbandRequest {
    std::uint32_t cookie = 0;
    bool operator==(const ClanDisbandRequest&) const = default;
};

/// SID_CLAN_MEMBERNEWCHIEF (0x74) — request: cookie + player_name.
struct ClanNewChiefRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    bool operator==(const ClanNewChiefRequest&) const = default;
};

/// SID_CLAN_INVITE (0x77) — request: cookie + invitee_name.
struct ClanInviteRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    bool operator==(const ClanInviteRequest&) const = default;
};

/// SID_CLANMEMBER_REMOVE (0x78) — request: cookie + player_name.
struct ClanMemberRemoveRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    bool operator==(const ClanMemberRemoveRequest&) const = default;
};

/// SID_CLANMEMBER_RANKUPDATE (0x7A) — request: cookie + player_name + new_rank.
struct ClanMemberRankUpdateRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    std::uint8_t  new_rank = 0;       ///< SERVER_CLAN_MEMBER_* (1=peon, 2=grunt, 3=shaman, 4=chieftain)
    bool operator==(const ClanMemberRankUpdateRequest&) const = default;
};

/// Generic cookie + result-byte reply used by 0x73, 0x74, 0x77, 0x78, 0x7A.
/// `sid` carries the SID code so the variant can disambiguate when needed;
/// equality only compares the wire payload (cookie, result).
struct ClanGenericResultReply {
    std::uint8_t  sid    = 0;
    std::uint32_t cookie = 0;
    std::uint8_t  result = 0;
    bool operator==(const ClanGenericResultReply&) const = default;
};

/// SID_CLAN_MOTDCHG (0x7B, client only) — unknown u32 + new MOTD cstring.
struct ClanMotdChange {
    std::uint32_t unknown1 = 0;
    std::string   motd;
    bool operator==(const ClanMotdChange&) const = default;
};

/// SID_CLAN_MOTD (0x7C) — request: cookie only.
struct ClanMotdRequest {
    std::uint32_t cookie = 0;
    bool operator==(const ClanMotdRequest&) const = default;
};

/// SID_CLAN_MOTD (0x7C) — reply: cookie + unknown u32 + motd cstring.
struct ClanMotdReply {
    std::uint32_t cookie   = 0;
    std::uint32_t unknown1 = 0;
    std::string   motd;
    bool operator==(const ClanMotdReply&) const = default;
};

// --- SID 0x71 / 0x72 / 0x79 — multi-cookie invite chains -----------------
//
// Creating a clan needs the founder to gather signatures from at least four
// friends. The wire conversation is:
//   client → 0x71 ClanCreateInviteRequest    (cookie A)
//   server → 0x72 ClanCreateInviteForward    (cookie B, to each friend)
//   client ← 0x72 ClanCreateInviteResponse   (cookie B, accept / decline)
//   server → 0x71 ClanCreateInviteSummary    (cookie A, status, failed name)
// After the clan exists, late joins use the simpler 0x79 pair.

/// 0x71 client → server: list of friends to enroll for a new clan.
struct ClanCreateInviteRequest {
    std::uint32_t            cookie   = 0;
    std::string              clan_name;
    std::uint32_t            clan_tag = 0;
    std::vector<std::string> friend_names;
    bool operator==(const ClanCreateInviteRequest&) const = default;
};

/// 0x71 server → client (founder): result of the enrollment poll.
/// `status`: 0x00 OK, 0x04 declined, 0x05 cannot-contact / already-in-clan.
/// `failed_member` carries the offending name when status != 0.
struct ClanCreateInviteSummary {
    std::uint32_t cookie = 0;
    std::uint8_t  status = 0;
    std::string   failed_member;     ///< empty on success
    bool operator==(const ClanCreateInviteSummary&) const = default;
};

/// 0x72 server → client (invited friend): forwarded poll question.
struct ClanCreateInviteForward {
    std::uint32_t            cookie       = 0;
    std::uint32_t            clan_tag     = 0;
    std::string              clan_name;
    std::string              clan_creator;
    std::vector<std::string> friend_names;
    bool operator==(const ClanCreateInviteForward&) const = default;
};

/// 0x72 client → server (invited friend): accept / decline.
/// `reply`: 0x04 decline, 0x05 cannot-contact / busy, 0x06 accept.
struct ClanCreateInviteResponse {
    std::uint32_t cookie       = 0;
    std::uint32_t clan_tag     = 0;
    std::string   clan_creator;
    std::uint8_t  reply        = 0;
    bool operator==(const ClanCreateInviteResponse&) const = default;
};

/// 0x79 server → client: late-join invite (clan already exists).
struct ClanInvite2Forward {
    std::uint32_t cookie    = 0;
    std::uint32_t clan_tag  = 0;
    std::string   clan_name;
    std::string   inviter_name;
    bool operator==(const ClanInvite2Forward&) const = default;
};

/// 0x79 client → server: late-join invite reply. `reply` uses the same codes
/// as `ClanCreateInviteResponse::reply`.
struct ClanInvite2Response {
    std::uint32_t cookie       = 0;
    std::uint32_t clan_tag     = 0;
    std::string   inviter_name;
    std::uint8_t  reply        = 0;
    bool operator==(const ClanInvite2Response&) const = default;
};

// --- SID_CLANMEMBERLIST (0x7D) / member-event notifies (0x7E, 0x7F) -------

/// 0x7D client → server: request the current clan member list. Cookie echoes
/// back in the reply.
struct ClanMemberListRequest {
    std::uint32_t cookie = 0;
    bool operator==(const ClanMemberListRequest&) const = default;
};

/// Single entry inside `ClanMemberListReply`. `rank` uses the SERVER_CLAN_MEMBER_*
/// constants (PEON=0x01 .. CHIEFTAIN=0x04, NEW=0x00). `online_status` uses the
/// SERVER_CLAN_MEMBER_OFFLINE/ONLINE/CHANNEL/GAME/PRIVATE_GAME constants.
/// `location` is the trailing string (channel/game name) — empty when offline.
struct ClanMemberEntry {
    std::string   name;
    std::uint8_t  rank          = 0;
    std::uint8_t  online_status = 0;
    std::string   location;
    bool operator==(const ClanMemberEntry&) const = default;
};

/// 0x7D server → client: the full clan roster.
struct ClanMemberListReply {
    std::uint32_t cookie = 0;
    std::vector<ClanMemberEntry> members;
    bool operator==(const ClanMemberListReply&) const = default;
};

/// 0x7E server → client: notify that a member was removed from the clan.
struct ClanMemberRemovedNotify {
    std::string name;
    bool operator==(const ClanMemberRemovedNotify&) const = default;
};

/// 0x7F server → client: update a member's online status.
/// `online_status` matches SERVER_CLAN_MEMBER_OFFLINE/ONLINE/CHANNEL/GAME/PRIVATE_GAME.
/// `location` is empty unless the member is in a channel or game.
struct ClanMemberUpdate {
    std::string   name;
    std::uint8_t  rank          = 0;
    std::uint8_t  online_status = 0;
    std::string   location;
    bool operator==(const ClanMemberUpdate&) const = default;
};

// --- SID_CLANINFO (0x82) — clan-membership query ---------------------------
struct ClanInfoRequest {
    std::uint32_t cookie   = 0;
    std::uint32_t clan_tag = 0;       ///< 4-char clan tag
    std::string   player_name;
    bool operator==(const ClanInfoRequest&) const = default;
};

/// On `fail == 0` the reply carries clan_name, rank, join_time.
/// On `fail != 0` the trailing fields are absent.
struct ClanInfoReply {
    std::uint32_t cookie    = 0;
    std::uint8_t  fail      = 0;
    std::string   clan_name;
    std::uint8_t  rank      = 0;
    std::uint32_t join_time = 0;
    bool operator==(const ClanInfoReply&) const = default;
};

// =========================================================================
// Legacy / OLS / pre-NLS protocol structs — implemented in the "all SIDs" pass.
// Wire formats follow legacy `src/common/bnet_protocol.h`. FSM treats most of
// these as advisory pre-login handshakes: handlers return `core::ok()`.
// =========================================================================

// --- CLIENT_COMPINFO1 / SERVER_COMPREPLY (0x05) -------------------------
struct CompInfo1Request {
    std::uint32_t reg_version  = 0x00000001u;
    std::uint32_t reg_auth     = 0xaa8843d1u;
    std::uint32_t client_id    = 0x001b9ddau;
    std::uint32_t client_token = 0xab69f79au;
    std::string   host;  ///< optional; empty when absent
    std::string   user;  ///< optional; empty when absent
    bool operator==(const CompInfo1Request&) const = default;
};

struct CompReply {
    std::uint32_t reg_version  = 0x00000001u;
    std::uint32_t reg_auth     = 0xaa8843d1u;
    std::uint32_t client_id    = 0x001b9ddau;
    std::uint32_t client_token = 0xab69f79au;
    bool operator==(const CompReply&) const = default;
};

// --- CLIENT_PROGIDENT / SERVER_AUTHREQ1 (0x06) --------------------------
struct ProgIdent {
    std::uint32_t archtag    = 0;  ///< e.g. "IX86"
    std::uint32_t clienttag  = 0;  ///< e.g. "STAR", "D2DV"
    std::uint32_t versionid  = 0;
    std::uint32_t unknown1   = 0;  ///< spawn flag / always-zero in captures
    bool operator==(const ProgIdent&) const = default;
};

struct AuthReq1Server {
    std::uint64_t timestamp = 0;  ///< file modification time
    std::string   filename;       ///< versioncheck filename
    std::string   equation;       ///< checksum equation
    bool operator==(const AuthReq1Server&) const = default;
};

// --- CLIENT_AUTHREQ1 / SERVER_AUTHREPLY1 (0x07) -------------------------
struct AuthReq1 {
    std::uint32_t archtag     = 0;
    std::uint32_t clienttag   = 0;
    std::uint32_t versionid   = 0;
    std::uint32_t gameversion = 0;
    std::uint32_t checksum    = 0;
    std::string   exeinfo;
    bool operator==(const AuthReq1&) const = default;
};

inline constexpr std::uint32_t kAuthReply1MessageBadVersion = 0x00000000u;
inline constexpr std::uint32_t kAuthReply1MessageUpdate     = 0x00000001u;
inline constexpr std::uint32_t kAuthReply1MessageOk         = 0x00000002u;

struct AuthReply1 {
    std::uint32_t message = kAuthReply1MessageOk;
    std::string   filename;  ///< optional patch filename (only on UPDATE)
    std::string   unknown;   ///< optional trailing string (rare)
    bool operator==(const AuthReply1&) const = default;
};

// --- CLIENT_COUNTRYINFO1 (0x12) -----------------------------------------
struct CountryInfo1 {
    std::uint64_t systemtime    = 0;  ///< GMT
    std::uint64_t localtime     = 0;  ///< local timezone
    std::int32_t  bias          = 0;  ///< (gmt-local)/60, signed minutes
    std::uint32_t langid1       = 0;
    std::uint32_t langid2       = 0;
    std::uint32_t langid3       = 0;
    std::string   langstr;            ///< e.g. "enu"
    std::string   countrycode;        ///< long-distance phone, e.g. "1"
    std::string   countryabbrev;      ///< e.g. "USA"
    std::string   countryname;        ///< e.g. "United States"
    bool operator==(const CountryInfo1&) const = default;
};

// --- SERVER_SESSIONKEY2 (0x1D) ------------------------------------------
struct SessionKey2 {
    std::uint32_t sessionnum = 0;
    std::uint32_t sessionkey = 0;
    bool operator==(const SessionKey2&) const = default;
};

// --- CLIENT_COMPINFO2 (0x1E) --------------------------------------------
struct CompInfo2 {
    std::uint32_t unknown1     = 0x00000001u;
    std::uint32_t reg_version  = 0x00000001u;
    std::uint32_t reg_auth     = 0xaa8843d1u;
    std::uint32_t client_id    = 0x001b9ddau;
    std::uint32_t client_token = 0xab69f79au;
    std::string   host;
    std::string   user;
    bool operator==(const CompInfo2&) const = default;
};

// --- SERVER_SESSIONKEY1 (0x28) ------------------------------------------
struct SessionKey1 {
    std::uint32_t sessionkey = 0;
    bool operator==(const SessionKey1&) const = default;
};

// --- CLIENT_LOGINREQ1 / SERVER_LOGINREPLY1 (0x29) -----------------------
struct LoginReq1 {
    std::uint32_t                ticks      = 0;
    std::uint32_t                sessionkey = 0;
    std::array<std::uint32_t, 5> password_hash2{};
    std::string                  player_name;
    bool operator==(const LoginReq1&) const = default;
};

inline constexpr std::uint32_t kLoginReply1MessageFail    = 0x00000000u;
inline constexpr std::uint32_t kLoginReply1MessageSuccess = 0x00000001u;

struct LoginReply1 {
    std::uint32_t message = kLoginReply1MessageSuccess;
    bool operator==(const LoginReply1&) const = default;
};

// --- CLIENT_CREATEACCTREQ1 / SERVER_CREATEACCTREPLY1 (0x2A) -------------
struct CreateAccount1Request {
    std::array<std::uint32_t, 5> password_hash1{};
    std::string                  player_name;
    bool operator==(const CreateAccount1Request&) const = default;
};

inline constexpr std::uint32_t kCreateAccount1ResultNo = 0x00000000u;
inline constexpr std::uint32_t kCreateAccount1ResultOk = 0x00000001u;

struct CreateAccount1Reply {
    std::uint32_t result = kCreateAccount1ResultOk;
    bool operator==(const CreateAccount1Reply&) const = default;
};

// --- CLIENT_UNKNOWN_2B (0x2B) -------------------------------------------
struct Unknown2B {
    std::uint32_t unknown1 = 0x00000001u;
    std::uint32_t unknown2 = 0;
    std::uint32_t unknown3 = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::uint32_t unknown6 = 0;
    std::uint32_t unknown7 = 0;
    bool operator==(const Unknown2B&) const = default;
};

// --- CLIENT_CDKEY / SERVER_CDKEYREPLY (0x30) ----------------------------
struct CdKeyLegacyRequest {
    std::uint32_t spawn = 0;
    std::string   cdkey;
    std::string   owner_name;
    bool operator==(const CdKeyLegacyRequest&) const = default;
};

inline constexpr std::uint32_t kCdKeyLegacyMessageOk       = 0x00000001u;
inline constexpr std::uint32_t kCdKeyLegacyMessageBad      = 0x00000002u;
inline constexpr std::uint32_t kCdKeyLegacyMessageWrongApp = 0x00000003u;
inline constexpr std::uint32_t kCdKeyLegacyMessageError    = 0x00000004u;
inline constexpr std::uint32_t kCdKeyLegacyMessageInUse    = 0x00000005u;

struct CdKeyLegacyReply {
    std::uint32_t message = kCdKeyLegacyMessageOk;
    std::string   owner_name;
    bool operator==(const CdKeyLegacyReply&) const = default;
};

// --- CLIENT_CHANGEPASSREQ / SERVER_CHANGEPASSACK (0x31) -----------------
struct ChangePasswordRequest {
    std::uint32_t                ticks      = 0;
    std::uint32_t                sessionkey = 0;
    std::array<std::uint32_t, 5> oldpassword_hash2{};
    std::array<std::uint32_t, 5> newpassword_hash1{};
    std::string                  player_name;
    bool operator==(const ChangePasswordRequest&) const = default;
};

inline constexpr std::uint32_t kChangePasswordMessageFail    = 0x00000000u;
inline constexpr std::uint32_t kChangePasswordMessageSuccess = 0x00000001u;

struct ChangePasswordReply {
    std::uint32_t message = kChangePasswordMessageSuccess;
    bool operator==(const ChangePasswordReply&) const = default;
};

// --- CLIENT_UNKNOWN_39 (0x39) -------------------------------------------
// D2 post-character-delete advisory. Client only.
struct Unknown39 {
    std::string char_name;  ///< "RealmName,CharacterName" or open-realm char
    bool operator==(const Unknown39&) const = default;
};

// --- CLIENT_CREATEACCTREQ2 / SERVER_CREATEACCTREPLY2 (0x3D) -------------
struct CreateAccountRequest {
    std::array<std::uint32_t, 5> password_hash1{};
    std::string                  username;
    bool operator==(const CreateAccountRequest&) const = default;
};

inline constexpr std::uint32_t kCreateAccountResultOk              = 0x00000000u;
inline constexpr std::uint32_t kCreateAccountResultShort           = 0x00000001u;
inline constexpr std::uint32_t kCreateAccountResultInvalid         = 0x00000002u;
inline constexpr std::uint32_t kCreateAccountResultBanned          = 0x00000003u;
inline constexpr std::uint32_t kCreateAccountResultExist           = 0x00000004u;
inline constexpr std::uint32_t kCreateAccountResultInProgress      = 0x00000005u;
inline constexpr std::uint32_t kCreateAccountResultAlphanum        = 0x00000006u;
inline constexpr std::uint32_t kCreateAccountResultPunctuation     = 0x00000007u;
inline constexpr std::uint32_t kCreateAccountResultPunctuation2    = 0x00000008u;

struct CreateAccountReply {
    std::uint32_t result = kCreateAccountResultOk;
    bool operator==(const CreateAccountReply&) const = default;
};

// --- CLIENT_CHANGEGAMEPORT (0x45) ---------------------------------------
struct NetGamePort {
    std::uint16_t port = 0;
    bool operator==(const NetGamePort&) const = default;
};

// --- Game-lifecycle SIDs --------------------------------------------------

/// 0x02 / 0x1F client → server: empty body — "I left the game".
struct CloseGame {
    bool operator==(const CloseGame&) const = default;
};
struct CloseGame2 {
    bool operator==(const CloseGame2&) const = default;
};

/// 0x08 CLIENT_STARTGAME1 (original StarCraft / shareware).
struct StartGame1Request {
    std::uint32_t status   = 0;
    std::uint32_t unknown3 = 0;
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::string   game_name;
    std::string   password;
    std::string   info;
    bool operator==(const StartGame1Request&) const = default;
};

/// 0x08 SERVER_STARTGAME1_ACK. `reply` = 0 on success.
struct StartGame1Ack {
    std::uint32_t reply = 0;
    bool operator==(const StartGame1Ack&) const = default;
};

/// 0x1A CLIENT_STARTGAME3 (StarCraft 1.03 / Diablo 1.07).
struct StartGame3Request {
    std::uint32_t status   = 0;
    std::uint32_t unknown3 = 0;
    std::uint16_t gametype = 0;
    std::uint16_t unknown1 = 0;
    std::uint32_t unknown6 = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::string   game_name;
    std::string   password;
    std::string   info;
    bool operator==(const StartGame3Request&) const = default;
};

/// 0x1A SERVER_STARTGAME3_ACK. `reply` = 0 on success.
struct StartGame3Ack {
    std::uint32_t reply = 0;
    bool operator==(const StartGame3Ack&) const = default;
};

/// 0x22 CLIENT_JOIN_GAME: client tells server it joined game `game_name`.
struct JoinGame {
    std::uint32_t clienttag  = 0;
    std::uint32_t versiontag = 0;
    std::string   game_name;
    std::string   password;
    bool operator==(const JoinGame&) const = default;
};

/// 0x2C CLIENT_GAME_REPORT: per-player results + free-form report blob.
/// Wire layout: u32 unknown1, u32 count, count × u32 result, count × cstring
/// player name, cstring report_header, cstring report_body.
struct GameReport {
    std::uint32_t              unknown1 = 0;
    std::vector<std::uint32_t> results;       ///< one result per player slot
    std::vector<std::string>   player_names;  ///< parallel array, same size
    std::string                report_header;
    std::string                report_body;
    bool operator==(const GameReport&) const = default;
};
inline constexpr std::uint32_t kGameReportResultPlaying    = 0;
inline constexpr std::uint32_t kGameReportResultWin        = 1;
inline constexpr std::uint32_t kGameReportResultLoss       = 2;
inline constexpr std::uint32_t kGameReportResultDraw       = 3;
inline constexpr std::uint32_t kGameReportResultDisconnect = 4;
inline constexpr std::uint32_t kGameReportResultObserver   = 5;

// --- Misc / anti-cheat / advisory ---------------------------------------

/// 0x17 server → client: anti-cheat "read N bytes at address" request.
struct ReadMemoryRequest {
    std::uint32_t request_id = 0;
    std::uint32_t address    = 0;
    std::uint32_t length     = 0;
    bool operator==(const ReadMemoryRequest&) const = default;
};

/// 0x17 client → server: anti-cheat memory contents reply.
struct ReadMemoryReply {
    std::uint32_t             request_id = 0;
    std::vector<std::uint8_t> memory;
    bool operator==(const ReadMemoryReply&) const = default;
};

/// 0x1B client → server: post-join game UDP/IP advisory.
/// `port` and `ip` are stored on the wire in big-endian byte order; here
/// we keep them as raw u32/u16 fields to preserve byte-exact round-trip.
struct Unknown1B {
    std::uint16_t unknown1 = 0;
    std::uint16_t port_be  = 0;  ///< big-endian on wire; preserved verbatim
    std::uint32_t ip_be    = 0;  ///< big-endian on wire; preserved verbatim
    std::uint32_t unknown2 = 0;
    std::uint32_t unknown3 = 0;
    bool operator==(const Unknown1B&) const = default;
};

/// 0x24 client → server: empty advisory packet (purpose unknown).
struct Unknown24 {
    bool operator==(const Unknown24&) const = default;
};

/// 0x32 client → server: map-checksum auth request.
struct MapAuthReq1 {
    std::array<std::uint32_t, 5> file_checksum{};
    std::string                  mapfile;
    bool operator==(const MapAuthReq1&) const = default;
};

/// 0x32 server → client.
struct MapAuthReply1 {
    std::uint32_t response = 0;
    bool operator==(const MapAuthReply1&) const = default;
};
inline constexpr std::uint32_t kMapAuthReply1ResponseNo       = 0;
inline constexpr std::uint32_t kMapAuthReply1ResponseOk       = 1;
inline constexpr std::uint32_t kMapAuthReply1ResponseLadderOk = 2;

/// 0x3C client → server: map-checksum auth v2.
struct MapAuthReq2 {
    std::uint32_t                unknown = 0;
    std::array<std::uint32_t, 5> file_hash{};
    std::string                  mapfile;
    bool operator==(const MapAuthReq2&) const = default;
};

/// 0x3C server → client.
struct MapAuthReply2 {
    std::uint32_t response = 0;
    bool operator==(const MapAuthReply2&) const = default;
};

/// 0x5C client → server: switch active client tag (cross-game session).
struct ChangeClient {
    std::uint32_t clienttag = 0;
    bool operator==(const ChangeClient&) const = default;
};

// Message variants. Add new arms as new SIDs are wired up.
using ClientMessage = std::variant<
    Null,
    Ping,
    AuthInfo,
    AuthCheckRequest,
    LogonResponse2,
    JoinChannel,
    EnterChatRequest,
    ChatCommand,
    GameListRequest,
    LadderSearchRequest,
    FileInfoRequest,
    CdKey2Request,
    FriendsListRequest,
    FriendInfoRequest,
    ClanInfoRequest,
    UserDataReadRequest,
    UserDataWriteRequest,
    ClanCreateRequest,
    ClanDisbandRequest,
    ClanNewChiefRequest,
    ClanInviteRequest,
    ClanMemberRemoveRequest,
    ClanMemberRankUpdateRequest,
    ClanMotdChange,
    ClanMotdRequest,
    ClanCreateInviteRequest,
    ClanCreateInviteResponse,
    ClanInvite2Response,
    ClanMemberListRequest,
    ArrangedTeamFriendScreenRequest,
    ArrangedTeamInviteFriendRequest,
    ArrangedTeamAcceptDeclineInvite,
    ArrangedTeamAcceptInvite,
    StartGame4Request,
    UdpOk,
    LadderListRequest,
    AdRequest,
    AdClick,
    AdAck,
    AdClick2Request,
    MotdRequest,
    ChannelListRequest,
    LeaveChannel,
    RegSnoopReply,
    ProfileRequest,
    SetEmailReply,
    IconRequest,
    GetPasswordRequest,
    ChangeEmailRequest,
    CrashDump,
    CharListRequest,
    RealmListRequest,
    RealmJoinRequest,
    WarcraftGeneralRequest,
    ExtraWork,
    RealmListLegacyRequest,
    CdKey3Request,
    CreateAccount2Request,
    LoginW3Request,
    LogonProofW3Request,
    PassChangeRequest,
    PassChangeProofRequest,
    CompInfo1Request,
    ProgIdent,
    AuthReq1,
    CountryInfo1,
    CompInfo2,
    LoginReq1,
    CreateAccount1Request,
    Unknown2B,
    CdKeyLegacyRequest,
    ChangePasswordRequest,
    Unknown39,
    CreateAccountRequest,
    NetGamePort,
    CloseGame,
    CloseGame2,
    StartGame1Request,
    StartGame3Request,
    JoinGame,
    GameReport,
    ReadMemoryReply,
    Unknown1B,
    Unknown24,
    MapAuthReq1,
    MapAuthReq2,
    ChangeClient>;

using ServerMessage = std::variant<
    Null,
    Ping,
    AuthInfoReply,
    AuthCheckReply,
    LogonResponse2Reply,
    EnterChatReply,
    ChatEvent,
    GameListReply,
    LadderSearchReply,
    FileInfoReply,
    CdKey2Reply,
    FriendsListReply,
    FriendInfoReply,
    ClanInfoReply,
    UserDataReadReply,
    ClanCreateReply,
    ClanGenericResultReply,
    ClanMotdReply,
    ClanCreateInviteSummary,
    ClanCreateInviteForward,
    ClanInvite2Forward,
    ClanMemberListReply,
    ClanMemberRemovedNotify,
    ClanMemberUpdate,
    FriendAddAck,
    FriendDelAck,
    FriendMoveAck,
    ArrangedTeamFriendScreenReply,
    ArrangedTeamInviteFriendAck,
    ArrangedTeamMemberDecline,
    ArrangedTeamSendInvite,
    StartGame4Ack,
    LadderListReply,
    AdReply,
    AdClick2Reply,
    MotdReply,
    ChannelListReply,
    RegSnoopRequest,
    ProfileReply,
    SetEmailRequest,
    IconReply,
    CharListReply,
    ServerList,
    MessageBox,
    RealmListReply,
    RealmJoinReply,
    WarcraftGeneralReply,
    RequiredWork,
    RealmListLegacyReply,
    CdKey3Reply,
    CreateAccount2Reply,
    LoginW3Reply,
    LogonProofW3Reply,
    PassChangeReply,
    PassChangeProofReply,
    CompReply,
    AuthReq1Server,
    AuthReply1,
    SessionKey1,
    SessionKey2,
    LoginReply1,
    CreateAccount1Reply,
    CdKeyLegacyReply,
    ChangePasswordReply,
    CreateAccountReply,
    StartGame1Ack,
    StartGame3Ack,
    ReadMemoryRequest,
    MapAuthReply1,
    MapAuthReply2>;

}  // namespace pvpgn::protocol::bnet
