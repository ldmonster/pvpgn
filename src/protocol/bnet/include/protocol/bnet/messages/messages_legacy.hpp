// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_legacy.hpp
/// Legacy/OLS protocol messages: CompInfo, ProgIdent, AuthReq1, CountryInfo1, LoginReq1, etc.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages/messages_common.hpp"

namespace pvpgn::protocol::bnet {

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

/// SERVER_AUTHREPLY1 (0x07). Legacy on-wire layout:
///   u32 message
///   if !filename.empty(): cstring filename
///   cstring ""     // always emitted -- legacy parity
///   cstring ""     // always emitted -- legacy parity
struct AuthReply1 {
    std::uint32_t message = kAuthReply1MessageOk;
    std::string   filename;  ///< optional patch filename (only on UPDATE)
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

}  // namespace pvpgn::protocol::bnet
