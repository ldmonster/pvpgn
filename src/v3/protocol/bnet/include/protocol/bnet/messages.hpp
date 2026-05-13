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
inline constexpr std::uint8_t kSidEnterChat       = 0x0A;
inline constexpr std::uint8_t kSidJoinChannel     = 0x0C;
inline constexpr std::uint8_t kSidChatCommand     = 0x0E;
inline constexpr std::uint8_t kSidChatEvent       = 0x0F;
inline constexpr std::uint8_t kSidPing            = 0x25;  // ECHOREQ / ECHOREPLY
inline constexpr std::uint8_t kSidLogonResponse2  = 0x3A;
inline constexpr std::uint8_t kSidAuthInfo        = 0x50;
inline constexpr std::uint8_t kSidAuthCheck       = 0x51;

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

/// SID_AUTH_CHECK (server → client) reply.
/// `result` codes: 0 = passed, others fail with a reason string.
struct AuthCheckReply {
    std::uint32_t result = 0;
    std::string   info;       ///< MPQ name / reason / "" on success.
    bool operator==(const AuthCheckReply&) const = default;
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

// Message variants. Add new arms as new SIDs are wired up.
using ClientMessage = std::variant<
    Null,
    Ping,
    AuthInfo,
    LogonResponse2,
    JoinChannel,
    EnterChatRequest,
    ChatCommand>;

using ServerMessage = std::variant<
    Null,
    Ping,
    AuthCheckReply,
    LogonResponse2Reply,
    EnterChatReply,
    ChatEvent>;

}  // namespace pvpgn::protocol::bnet
