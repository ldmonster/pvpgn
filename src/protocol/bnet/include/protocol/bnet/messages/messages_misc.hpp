// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_misc.hpp
/// Misc messages: Ad, Motd, ChannelList, RegSnoop, Profile, Email, Icon, etc.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages_common.hpp"

namespace pvpgn::protocol::bnet {

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
//
// On Windows, <windows.h> (pulled in transitively by Boost.Asio) does
// `#define MessageBox MessageBoxA`, which would rename this type only in TUs
// that include windows.h — mangling its symbol and breaking linkage against
// TUs that don't. Neutralise the macro so the type is consistently `MessageBox`.
#ifdef MessageBox
#  undef MessageBox
#endif
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

}  // namespace pvpgn::protocol::bnet
