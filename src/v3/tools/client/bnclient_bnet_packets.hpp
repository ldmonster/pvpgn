// SPDX-License-Identifier: GPL-2.0-or-later
//
// Vendored BNet protocol packet bodies needed by the v3 client tools
// (`bnstat`, `bnchat`).  Header-only.
//
// Each `struct` below is the body that follows the 4-byte BNet
// header `{ type, size }` — the framing layer in
// `bnclient_proto.hpp` (`Packet::set_bnet_type / set_bnet_size`)
// owns the header bytes, so these PODs deliberately do NOT embed it.
// Use them via `Packet::body_as<T>()`.
//
// All scalar fields are the vendored little-endian
// `bn_byte` / `bn_short` / `bn_int` byte arrays from
// `bnclient_proto.hpp`.  Layouts are packed via
// `#pragma pack(push, 1)` so the structs match the wire byte-for-
// byte across compilers.
//
// Source of truth: `src/common/bnet_protocol.h` and `tag.h`.  This
// header copies only the surface the four client binaries actually
// need.

#ifndef PVPGN_V3_CLIENT_BNCLIENT_BNET_PACKETS_HPP
#define PVPGN_V3_CLIENT_BNCLIENT_BNET_PACKETS_HPP

#include <cstddef>
#include <cstdint>

#include "bnclient_proto.hpp"

namespace pvpgn::client_v3::bnet {

using proto::bn_byte;
using proto::bn_short;
using proto::bn_int;
using proto::bn_long;

// ---- packet type IDs --------------------------------------------------
//
// Names mirror the legacy `CLIENT_*` / `SERVER_*` macros so a side-by-
// side review against `common/bnet_protocol.h` is trivial.

namespace packet_id {

inline constexpr std::uint16_t SERVER_PINGREPLY    = 0x00ff;
inline constexpr std::uint16_t CLIENT_PINGREQ      = 0x00ff;
inline constexpr std::uint16_t CLIENT_PROGIDENT    = 0x06ff;
inline constexpr std::uint16_t SERVER_AUTHREQ1     = 0x06ff;
inline constexpr std::uint16_t CLIENT_AUTHREQ1     = 0x07ff;
inline constexpr std::uint16_t SERVER_AUTHREPLY1   = 0x07ff;
inline constexpr std::uint16_t CLIENT_UNKNOWN_1B   = 0x1bff;
inline constexpr std::uint16_t CLIENT_ECHOREPLY    = 0x25ff;
inline constexpr std::uint16_t SERVER_ECHOREQ      = 0x25ff;
inline constexpr std::uint16_t CLIENT_STATSREQ     = 0x26ff;
inline constexpr std::uint16_t SERVER_STATSREPLY   = 0x26ff;
inline constexpr std::uint16_t CLIENT_LOGINREQ1    = 0x29ff;
inline constexpr std::uint16_t SERVER_LOGINREPLY1  = 0x29ff;
inline constexpr std::uint16_t CLIENT_CREATEACCTREQ1   = 0x2aff;
inline constexpr std::uint16_t SERVER_CREATEACCTREPLY1 = 0x2aff;
inline constexpr std::uint16_t CLIENT_ICONREQ      = 0x2dff;
inline constexpr std::uint16_t SERVER_ICONREPLY    = 0x2dff;
inline constexpr std::uint16_t CLIENT_CDKEY2       = 0x36ff;
inline constexpr std::uint16_t SERVER_CDKEYREPLY2  = 0x36ff;
inline constexpr std::uint16_t CLIENT_AUTH_INFO    = 0x50ff;
inline constexpr std::uint16_t SERVER_AUTHREPLY_109= 0x51ff;
inline constexpr std::uint16_t CLIENT_AUTHREQ_109  = 0x51ff;
inline constexpr std::uint16_t SERVER_AUTHREQ_109  = 0x50ff;

// Chat-class packet ids (used by `bnchat_v3`).
inline constexpr std::uint16_t CLIENT_PROGIDENT2   = 0x0bff;
inline constexpr std::uint16_t SERVER_CHANNELLIST  = 0x0bff;
inline constexpr std::uint16_t CLIENT_JOINCHANNEL  = 0x0cff;
inline constexpr std::uint16_t CLIENT_MESSAGE      = 0x0eff;
inline constexpr std::uint16_t SERVER_MESSAGE      = 0x0fff;

} // namespace packet_id

// ---- tag literals (4cc, ASCII) ----------------------------------------

namespace tag {

inline constexpr char archtag_IX86[4]  = {'I','X','8','6'};
inline constexpr char gamelang_enUS[4] = {'e','n','U','S'};

inline constexpr char clienttag_STAR[4] = {'S','T','A','R'};  // Starcraft
inline constexpr char clienttag_SEXP[4] = {'S','E','X','P'};  // Brood War
inline constexpr char clienttag_SSHR[4] = {'S','S','H','R'};  // Starcraft Shareware
inline constexpr char clienttag_DRTL[4] = {'D','R','T','L'};  // Diablo Retail
inline constexpr char clienttag_DSHR[4] = {'D','S','H','R'};  // Diablo Shareware
inline constexpr char clienttag_W2BN[4] = {'W','2','B','N'};  // WarCraft II BNE
inline constexpr char clienttag_D2DV[4] = {'D','2','D','V'};  // Diablo II
inline constexpr char clienttag_D2XP[4] = {'D','2','X','P'};  // Diablo II LoD
inline constexpr char clienttag_WAR3[4] = {'W','A','R','3'};  // Warcraft III
inline constexpr char clienttag_CHAT[4] = {'C','H','A','T'};  // chat bot

} // namespace tag

// ---- packet body PODs -------------------------------------------------
//
// Each body is the wire bytes that follow the 4-byte BNet header.
// Pack tightly so no compiler padding sneaks in.

#pragma pack(push, 1)

// CLIENT_AUTH_INFO 0x50ff -- followed by langstr + countryname C
// strings appended via Packet::append_cstr().
struct CClientAuthInfo {
    bn_int protocol;   // always 0
    bn_int archtag;    // "IX86" via int_tag_set
    bn_int clienttag;  // "STAR", "D2XP", ... via int_tag_set
    bn_int versionid;
    bn_int gamelang;   // "enUS" via int_tag_set (or 0)
    bn_int localip;    // 0
    bn_int bias;       // (gmt - local) / 60, signed (2's complement)
    bn_int lcid;       // Win32 LCID
    bn_int langid;     // Win32 LangID
};
static_assert(sizeof(CClientAuthInfo) == 36);

inline constexpr std::uint32_t kAuthInfoLangIdUsEnglish = 0x00000409u;
inline constexpr const char    kAuthInfoLangStrUsEnglish[] = "enUS";
inline constexpr const char    kAuthInfoCountryUsa[]        = "United States";
inline constexpr const char    kAuthInfoLangStrLowerUsEnglish[] = "ENG";

// SERVER_AUTHREPLY_109 0x51ff
struct SServerAuthReply109 {
    bn_int message;
};
static_assert(sizeof(SServerAuthReply109) == 4);

inline constexpr std::uint32_t kAuthReply109_Ok         = 0x00000000u;
inline constexpr std::uint32_t kAuthReply109_Update     = 0x00000100u;
inline constexpr std::uint32_t kAuthReply109_BadVersion = 0x00000101u;

// SERVER_AUTHREQ_109 0x50ff (server -> client challenge for 1.09+)
// Header is the 0x50ff type from the server side; the body is:
struct SServerAuthReq109 {
    bn_int  logontype;    // 0, or 2 for W3/W3XP
    bn_int  sessionkey;
    bn_int  sessionnum;
    bn_long timestamp;
    // followed by 2 NUL-terminated strings: versioncheck filename + equation
};
static_assert(sizeof(SServerAuthReq109) == 20);

inline constexpr std::uint32_t kAuthReq109_LogonType      = 0x00000000u;
inline constexpr std::uint32_t kAuthReq109_LogonType_W3   = 0x00000002u;

// CLIENT_AUTHREQ_109 0x51ff (response to challenge)
struct CClientAuthReq109 {
    bn_int ticks;
    bn_int gameversion;
    bn_int checksum;
    bn_int cdkey_number;   // 1 for D2, 2 for LoD
    bn_int spawn;          // 1 if spawn copy
    // followed by per-cdkey blocks + executable info string + cdkey owner
};
static_assert(sizeof(CClientAuthReq109) == 20);

// SERVER_AUTHREQ1 0x06ff
struct SServerAuthReq1 {
    bn_long timestamp;
    // followed by versioncheck filename + equation NUL-terminated strings
};
static_assert(sizeof(SServerAuthReq1) == 8);

// CLIENT_AUTHREQ1 0x07ff
struct CClientAuthReq1 {
    bn_int archtag;
    bn_int clienttag;
    bn_int versionid;
    bn_int gameversion;
    bn_int checksum;
    // followed by executable info NUL-terminated string
};
static_assert(sizeof(CClientAuthReq1) == 20);

// SERVER_AUTHREPLY1 0x07ff
struct SServerAuthReply1 {
    bn_int message;
    // optional filename + unknown trail
};
static_assert(sizeof(SServerAuthReply1) == 4);

inline constexpr std::uint32_t kAuthReply1_BadVersion = 0x00000000u;
inline constexpr std::uint32_t kAuthReply1_Update     = 0x00000001u;
inline constexpr std::uint32_t kAuthReply1_Ok         = 0x00000002u;

// CLIENT_UNKNOWN_1B 0x1bff (sent by Diablo classic before AUTH_INFO)
struct CClientUnknown1B {
    bn_short unknown1;     // 0x0002
    bn_short port;         // big-endian -- use short_nset
    bn_int   ip;           // big-endian -- use int_nset (0 = none)
    bn_int   unknown2;
    bn_int   unknown3;
};
static_assert(sizeof(CClientUnknown1B) == 16);

inline constexpr std::uint16_t kUnknown1B_Unknown1 = 0x0002;

// CLIENT_LOGINREQ1 0x29ff -- account name appended after the struct.
struct CClientLoginReq1 {
    bn_int ticks;
    bn_int sessionkey;
    bn_int password_hash2[5];  // 20 bytes: hash(ticks || sessionkey || hash1)
    // followed by player name NUL-terminated
};
static_assert(sizeof(CClientLoginReq1) == 8 + 5 * 4);

// SERVER_LOGINREPLY1 0x29ff
struct SServerLoginReply1 {
    bn_int message;
};
static_assert(sizeof(SServerLoginReply1) == 4);

inline constexpr std::uint32_t kLoginReply1_Fail    = 0x00000000u;
inline constexpr std::uint32_t kLoginReply1_Success = 0x00000001u;

// CLIENT_CREATEACCTREQ1 0x2aff -- account name appended after the struct.
// R197: used by bnchat_v3's `--create-account` flag to bootstrap an
// account against a fresh bnetd before LOGINREQ1.
struct CClientCreateAcctReq1 {
    bn_int password_hash1[5];  // 20 bytes: bnet_hash(lowercase(password))
    // followed by player name NUL-terminated
};
static_assert(sizeof(CClientCreateAcctReq1) == 5 * 4);

// SERVER_CREATEACCTREPLY1 0x2aff
struct SServerCreateAcctReply1 {
    bn_int result;
};
static_assert(sizeof(SServerCreateAcctReply1) == 4);

inline constexpr std::uint32_t kCreateAcctReply1_No = 0x00000000u;
inline constexpr std::uint32_t kCreateAcctReply1_Ok = 0x00000001u;

// CLIENT_STATSREQ 0x26ff -- name + N field key strings appended
struct CClientStatsReq {
    bn_int name_count;     // always 1 ("which players")
    bn_int key_count;
    bn_int requestid;      // echoed back; pick any sentinel, e.g. 0x02825278
    // followed by player name NUL-terminated, then N field keys
};
static_assert(sizeof(CClientStatsReq) == 12);

inline constexpr std::uint32_t kStatsReqRequestId = 0x02825278u;

// SERVER_STATSREPLY 0x26ff -- N field values (NUL-terminated strings) follow
struct SServerStatsReply {
    bn_int name_count;
    bn_int key_count;
    bn_int requestid;
    // followed by N field value strings
};
static_assert(sizeof(SServerStatsReply) == 12);

// CLIENT_ECHOREPLY 0x25ff / SERVER_ECHOREQ 0x25ff
struct CClientEchoReply {
    bn_int ticks;
};
static_assert(sizeof(CClientEchoReply) == 4);

struct SServerEchoReq {
    bn_int ticks;
};
static_assert(sizeof(SServerEchoReq) == 4);

// CLIENT_PINGREQ 0x00ff (header only, no body)
// SERVER_PINGREPLY 0x00ff (header only)

// CLIENT_ICONREQ 0x2dff (header only, no body)

// SERVER_ICONREPLY 0x2dff
struct SServerIconReply {
    bn_long timestamp;
    // followed by filename NUL-terminated
};
static_assert(sizeof(SServerIconReply) == 8);

// CLIENT_AUTHREQ_109 cdkey block (one entry per cdkey).
struct CdkeyInfo {
    bn_int len;
    bn_int type;
    bn_int checksum;
    bn_int u1;
    bn_int hash[5];
};
static_assert(sizeof(CdkeyInfo) == 36);

// CLIENT_CDKEY2 0x36ff -- cdkey-owner string appended.
struct CClientCdkey2 {
    bn_int spawn;
    bn_int keylen;       // strlen without NUL
    bn_int productid;
    bn_int keyvalue1;
    bn_int sessionkey;
    bn_int ticks;
    bn_int key_hash[5];
    // followed by owner name NUL-terminated
};
static_assert(sizeof(CClientCdkey2) == 6 * 4 + 5 * 4);

inline constexpr std::uint32_t kCdkey2_Spawn_True  = 0x00000001u;
inline constexpr std::uint32_t kCdkey2_Spawn_False = 0x00000000u;

// SERVER_CDKEYREPLY2 0x36ff
struct SServerCdkeyReply2 {
    bn_int message;
    // optional owner name string
};
static_assert(sizeof(SServerCdkeyReply2) == 4);

inline constexpr std::uint32_t kCdkeyReply2_Ok       = 0x00000001u;
inline constexpr std::uint32_t kCdkeyReply2_Bad      = 0x00000002u;
inline constexpr std::uint32_t kCdkeyReply2_WrongApp = 0x00000003u;
inline constexpr std::uint32_t kCdkeyReply2_Error    = 0x00000004u;
inline constexpr std::uint32_t kCdkeyReply2_InUse    = 0x00000005u;

// CLIENT_PROGIDENT 0x06ff -- legacy version of AUTH_INFO used by
// older clients (Starcraft 1.04 and below, Diablo).
struct CClientProgIdent {
    bn_int archtag;
    bn_int clienttag;
    bn_int versionid;
    bn_int unknown1;
};
static_assert(sizeof(CClientProgIdent) == 16);

// ---- Chat-class PODs (used by `bnchat_v3`) ---------------------------

// CLIENT_PROGIDENT2 0x0bff -- sent right after LOGINREPLY1_Success
// to advertise the client tag and unlock the chat surface.
struct CClientProgIdent2 {
    bn_int clienttag;
};
static_assert(sizeof(CClientProgIdent2) == 4);

// CLIENT_JOINCHANNEL 0x0cff -- followed by NUL-terminated channel
// name.
struct CClientJoinChannel {
    bn_int channelflag;
};
static_assert(sizeof(CClientJoinChannel) == 4);

inline constexpr std::uint32_t kJoinChannel_Normal  = 0x00000000u;
inline constexpr std::uint32_t kJoinChannel_Generic = 0x00000001u;
inline constexpr std::uint32_t kJoinChannel_Create  = 0x00000002u;

// SERVER_MESSAGE 0x0fff -- followed by NUL-terminated player name
// then NUL-terminated text (for non-TALK/EMOTE events the strings
// may be empty).
struct SServerMessage {
    bn_int type;
    bn_int flags;
    bn_int latency;
    bn_int player_ip;
    bn_int account_num;
    bn_int reg_auth;
};
static_assert(sizeof(SServerMessage) == 24);

inline constexpr std::uint32_t kMsgType_AddUser    = 0x00000001u;
inline constexpr std::uint32_t kMsgType_Join       = 0x00000002u;
inline constexpr std::uint32_t kMsgType_Part       = 0x00000003u;
inline constexpr std::uint32_t kMsgType_Whisper    = 0x00000004u;
inline constexpr std::uint32_t kMsgType_Talk       = 0x00000005u;
inline constexpr std::uint32_t kMsgType_Broadcast  = 0x00000006u;
inline constexpr std::uint32_t kMsgType_Channel    = 0x00000007u;
inline constexpr std::uint32_t kMsgType_UserFlags  = 0x00000009u;
inline constexpr std::uint32_t kMsgType_WhisperAck = 0x0000000au;
inline constexpr std::uint32_t kMsgType_Info       = 0x00000012u;
inline constexpr std::uint32_t kMsgType_Error      = 0x00000013u;
inline constexpr std::uint32_t kMsgType_Emote      = 0x00000017u;

#pragma pack(pop)

// ---- DRTL stats reply field constants (used by bnstat -d) -------------

namespace drtl {
    inline constexpr std::uint32_t class_warrior  = 0;
    inline constexpr std::uint32_t class_rogue    = 1;
    inline constexpr std::uint32_t class_sorcerer = 2;
}

} // namespace pvpgn::client_v3::bnet

#endif // PVPGN_V3_CLIENT_BNCLIENT_BNET_PACKETS_HPP
