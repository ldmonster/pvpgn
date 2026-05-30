// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_auth.hpp
/// Auth messages: AuthInfo, AuthCheck, LogonResponse2, CdKey2.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages_common.hpp"

namespace pvpgn::protocol::bnet {

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

/// SID_AUTH_INFO (server -> client) -- legacy `SERVER_AUTHREQ_109` (0x50).
/// Carries the version-check seed and MPQ filename, server token used
/// for the password hash, and the W3 logon-type flag.
struct AuthInfoReply {
    std::uint32_t logontype    = 0;   ///< 0 = std, 2 = W3/W3XP (NLS).
    std::uint32_t server_token = 0;   ///< aka sessionkey.
    std::uint32_t session_num  = 0;
    std::uint64_t timestamp    = 0;   ///< Windows FILETIME.
    std::string   mpq_filename;       ///< e.g. "ver-IX86-1.mpq"
    std::string   checksum_formula;   ///< version-check equation
    /// Optional trailing opaque bytes appended after the equation.
    /// Legacy bnetd emits a 128-byte zero pad here for W3/W3XP
    /// clients (server signature placeholder). Empty for all other
    /// clients. Stored as raw bytes so the codec stays oblivious to
    /// the (unused) signature payload format.
    std::vector<std::uint8_t> server_signature;
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

}  // namespace pvpgn::protocol::bnet
