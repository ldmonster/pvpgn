// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_realm.hpp
/// Realm messages: RealmList, RealmJoin, WarcraftGeneral, ExtraWork, RequiredWork.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages_common.hpp"

namespace pvpgn::protocol::bnet {

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

}  // namespace pvpgn::protocol::bnet
