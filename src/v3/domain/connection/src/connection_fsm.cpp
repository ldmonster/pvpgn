// SPDX-License-Identifier: GPL-2.0-or-later
#include "domain/connection/connection_fsm.hpp"

#include <array>
#include <cstring>
#include <span>

#include "core/error.hpp"

namespace pvpgn::domain::connection {

// ---------------------------------------------------------------------------
// Internal helpers — BNCS packet building
// ---------------------------------------------------------------------------

namespace {

/// Write a little-endian uint32 into a byte buffer at offset.
void write_le32(std::vector<std::byte>& buf, std::uint32_t v) {
    buf.push_back(std::byte{static_cast<std::uint8_t>( v        & 0xFFu)});
    buf.push_back(std::byte{static_cast<std::uint8_t>((v >>  8) & 0xFFu)});
    buf.push_back(std::byte{static_cast<std::uint8_t>((v >> 16) & 0xFFu)});
    buf.push_back(std::byte{static_cast<std::uint8_t>((v >> 24) & 0xFFu)});
}

/// Read a little-endian uint32 from a span at offset (returns 0 if OOB).
[[nodiscard]] std::uint32_t read_le32(std::span<const std::byte> s,
                                       std::size_t offset) noexcept {
    if (offset + 4 > s.size()) return 0u;
    return static_cast<std::uint32_t>(s[offset])
         | (static_cast<std::uint32_t>(s[offset + 1]) <<  8)
         | (static_cast<std::uint32_t>(s[offset + 2]) << 16)
         | (static_cast<std::uint32_t>(s[offset + 3]) << 24);
}

/// Read a null-terminated C-string from a span at offset.
/// Returns empty string if offset is out of bounds.
[[nodiscard]] std::string read_cstring(std::span<const std::byte> s,
                                        std::size_t offset) {
    if (offset >= s.size()) return {};
    const auto* begin = reinterpret_cast<const char*>(s.data() + offset);
    const auto* end   = reinterpret_cast<const char*>(s.data() + s.size());
    const auto* nul   = static_cast<const char*>(std::memchr(begin, '\0',
                                                  static_cast<std::size_t>(end - begin)));
    if (!nul) return std::string{begin, end};
    return std::string{begin, nul};
}

}  // namespace

// ---------------------------------------------------------------------------
// ConnectionFsm — public API
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::dispatch(std::uint8_t packet_id,
                                        std::span<const std::byte> payload) {
    if (state_ == ConnectionState::Disconnecting) {
        // Silently drop all packets once we are shutting down.
        return core::ok();
    }

    switch (packet_id) {
        // Keepalive — legal in every non-Disconnecting state
        case sid::kNull:
            return core::ok();

        // Ping echo — legal in every non-Disconnecting state
        case sid::kPing: {
            // Echo the 4-byte cookie verbatim
            std::vector<std::byte> body;
            body.reserve(4);
            const std::uint32_t cookie = read_le32(payload, 0);
            write_le32(body, cookie);
            return ctx_.send_packet(sid::kPing,
                                    std::span<const std::byte>{body});
        }

        // --- Connecting state ---
        case sid::kAuthInfo:
            return on_auth_info(payload);

        case sid::kLogonRequest:
        case sid::kLogonRequest2:
            return on_logon_request(payload);

        // --- Authenticating state ---
        case sid::kAuthCheck:
            return on_auth_check(payload);

        case sid::kAuthAccountLogon:
            return on_auth_accountlogon(payload);

        case sid::kAuthAccountLogonProof:
            return on_auth_accountlogonproof(payload);

        // --- LoggedIn state ---
        case sid::kEnterChat:
            return on_enter_chat(payload);

        // --- InChannel state ---
        case sid::kJoinChannel:
            return on_join_channel(payload);

        case sid::kChatCommand:
            return on_chat_command(payload);

        case sid::kLeaveChannel:
            return on_leave_channel(payload);

        case sid::kStartGame1:
        case sid::kStartGame3:
            return on_start_game(payload);

        case sid::kJoinGame:
            return on_join_game(payload);

        // --- InGame state ---
        case sid::kCloseGame:
            return on_leave_game(payload);

        default:
            // Unknown / unimplemented packet — silently ignore.
            // This is intentional: the strangler-fig bridge may handle it,
            // or it may be a future SID not yet migrated.
            return core::ok();
    }
}

void ConnectionFsm::close() {
    state_ = ConnectionState::Disconnecting;
    ctx_.close();
}

// ---------------------------------------------------------------------------
// Connecting state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_auth_info(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::Connecting) {
        return reject("connection_fsm: SID_AUTH_INFO out of order");
    }

    // SID_AUTH_INFO body layout (minimum 20 bytes):
    //   [0..3]   protocol_id   (LE uint32, should be 0)
    //   [4..7]   platform_id   (LE uint32, e.g. 'IX86')
    //   [8..11]  product_id    (LE uint32, e.g. 'STAR', 'WAR3')
    //   [12..15] version_byte  (LE uint32)
    //   [16..19] language_id   (LE uint32)
    //   [20..23] local_ip      (LE uint32)
    //   [24..27] time_zone_bias(LE int32)
    //   [28..31] mpq_locale_id (LE uint32)
    //   [32..35] user_language (LE uint32)
    //   [36..]   country_abbrev (NUL-terminated)
    //   [..]     country        (NUL-terminated)

    if (payload.size() >= 12) {
        client_product_tag_ = read_le32(payload, 8);
    }

    // Transition to Authenticating
    state_ = ConnectionState::Authenticating;

    // Reply: SID_AUTH_INFO (0x50)
    // Body layout:
    //   [0..3]   logon_type    (0 = Broken SHA-1 / OLS, 2 = NLS)
    //   [4..7]   server_token  (random 32-bit)
    //   [8..11]  udp_value     (0)
    //   [12..19] mpq_filetime  (8 bytes, 0 for now)
    //   [20..]   mpq_filename  (NUL-terminated, empty for now)
    //   [..]     formula       (NUL-terminated, empty for now)
    std::vector<std::byte> reply;
    reply.reserve(24);
    write_le32(reply, 2u);          // logon_type = NLS (SRP)
    write_le32(reply, 0xDEADBEEFu); // server_token (placeholder)
    write_le32(reply, 0u);          // udp_value
    // mpq_filetime (8 bytes, zero)
    for (int i = 0; i < 8; ++i) reply.push_back(std::byte{0});
    // mpq_filename (empty NUL-terminated)
    reply.push_back(std::byte{0});
    // formula (empty NUL-terminated)
    reply.push_back(std::byte{0});

    return ctx_.send_packet(sid::kAuthInfo,
                            std::span<const std::byte>{reply});
}

core::Status<> ConnectionFsm::on_auth_check(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::Authenticating) {
        // Silently ignore if out of order (some clients send this late)
        return core::ok();
    }

    // SID_AUTH_CHECK body:
    //   [0..3]   client_token  (LE uint32)
    //   [4..7]   exe_version   (LE uint32)
    //   [8..11]  exe_hash      (LE uint32)
    //   [12..15] num_keys      (LE uint32)
    //   [16..19] spawn         (LE uint32, 0 = not spawn)
    //   [20..]   cd_key data   (variable)
    //   [..]     exe_info      (NUL-terminated)
    //   [..]     key_owner     (NUL-terminated)

    // For now: accept all version checks (Phase 5 will add real policy).
    // Reply: SID_AUTH_CHECK (0x51)
    // Body:
    //   [0..3]   result  (0 = passed)
    //   [4..]    info    (NUL-terminated, empty on success)
    std::vector<std::byte> reply;
    write_le32(reply, 0u);   // result = passed
    reply.push_back(std::byte{0}); // empty info string

    return ctx_.send_packet(sid::kAuthCheck,
                            std::span<const std::byte>{reply});
}

core::Status<> ConnectionFsm::on_logon_request(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::Connecting) {
        return reject("connection_fsm: SID_LOGON_REQUEST out of order");
    }

    // SID_LOGONRESPONSE (0x29) / SID_LOGONRESPONSE2 (0x3A) body:
    //   [0..3]   client_token  (LE uint32)
    //   [4..7]   server_token  (LE uint32)
    //   [8..27]  password_hash (5 × LE uint32, Broken-SHA1)
    //   [28..]   username      (NUL-terminated)

    const std::string uname = read_cstring(payload, 28);
    if (uname.empty()) {
        // Reject empty username
        std::vector<std::byte> reply;
        write_le32(reply, 0x01u); // result = invalid password
        return ctx_.send_packet(sid::kLogonRequest,
                                std::span<const std::byte>{reply});
    }

    // Accept the legacy login (no real credential check in this skeleton).
    // Phase 5 will wire in the LoginUser use-case.
    username_   = uname;
    account_id_ = 1u; // placeholder
    state_      = ConnectionState::LoggedIn;

    // Reply: SID_LOGONRESPONSE (0x29) or SID_LOGONRESPONSE2 (0x3A)
    // Body: [0..3] result (0 = success)
    std::vector<std::byte> reply;
    write_le32(reply, 0u); // success

    return ctx_.send_packet(sid::kLogonRequest,
                            std::span<const std::byte>{reply});
}

// ---------------------------------------------------------------------------
// Authenticating state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_auth_accountlogon(
    std::span<const std::byte> payload) {
    if (state_ != ConnectionState::Authenticating) {
        return reject("connection_fsm: SID_AUTH_ACCOUNTLOGON out of order");
    }

    // SID_AUTH_ACCOUNTLOGON (0x53) body:
    //   [0..31]  client_key  (32 bytes, NLS SRP A value)
    //   [32..]   username    (NUL-terminated)

    const std::string uname = read_cstring(payload, 32);
    if (uname.empty()) {
        // Reject: no account name
        std::vector<std::byte> reply;
        write_le32(reply, 0x01u); // result = account does not exist
        // salt (32 bytes, zero)
        for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0});
        // server_key (32 bytes, zero)
        for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0});
        return ctx_.send_packet(sid::kAuthAccountLogon,
                                std::span<const std::byte>{reply});
    }

    // Store username; wait for PROOF before transitioning.
    username_ = uname;

    // Reply: SID_AUTH_ACCOUNTLOGON (0x53)
    // Body:
    //   [0..3]   result      (0 = success, account exists)
    //   [4..35]  salt        (32 bytes, placeholder zeros)
    //   [36..67] server_key  (32 bytes, placeholder zeros)
    std::vector<std::byte> reply;
    write_le32(reply, 0u); // result = success (account exists)
    for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0}); // salt
    for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0}); // server_key

    return ctx_.send_packet(sid::kAuthAccountLogon,
                            std::span<const std::byte>{reply});
}

core::Status<> ConnectionFsm::on_auth_accountlogonproof(
    std::span<const std::byte> payload) {
    if (state_ != ConnectionState::Authenticating) {
        return reject("connection_fsm: SID_AUTH_ACCOUNTLOGONPROOF out of order");
    }

    // SID_AUTH_ACCOUNTLOGONPROOF (0x54) body:
    //   [0..19]  client_proof  (20 bytes, NLS SRP M1)

    // Accept all proofs in this skeleton (Phase 5 will add real SRP).
    account_id_ = 1u; // placeholder
    state_      = ConnectionState::LoggedIn;

    // Reply: SID_AUTH_ACCOUNTLOGONPROOF (0x54)
    // Body:
    //   [0..3]   result        (0 = success)
    //   [4..23]  server_proof  (20 bytes, NLS SRP M2, zeros for now)
    std::vector<std::byte> reply;
    write_le32(reply, 0u); // success
    for (int i = 0; i < 20; ++i) reply.push_back(std::byte{0}); // server_proof

    return ctx_.send_packet(sid::kAuthAccountLogonProof,
                            std::span<const std::byte>{reply});
}

// ---------------------------------------------------------------------------
// LoggedIn state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_enter_chat(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::LoggedIn) {
        return reject("connection_fsm: SID_ENTERCHAT out of order");
    }

    // SID_ENTERCHAT (0x0A) body:
    //   [0..]  username    (NUL-terminated, may differ from login name)
    //   [..]   statstring  (NUL-terminated)

    const std::string chat_name = read_cstring(payload, 0);
    const std::string statstr   = read_cstring(payload,
                                               chat_name.size() + 1);

    // Use the login username if the chat name is empty
    const std::string& effective_name =
        chat_name.empty() ? username_ : chat_name;

    state_ = ConnectionState::InChannel;

    // Reply: SID_ENTERCHAT (0x0A)
    // Body:
    //   [0..]  unique_name  (NUL-terminated)
    //   [..]   statstring   (NUL-terminated)
    //   [..]   account_name (NUL-terminated)
    std::vector<std::byte> reply;
    for (char c : effective_name) reply.push_back(std::byte{static_cast<std::uint8_t>(c)});
    reply.push_back(std::byte{0}); // NUL
    for (char c : statstr) reply.push_back(std::byte{static_cast<std::uint8_t>(c)});
    reply.push_back(std::byte{0}); // NUL
    for (char c : username_) reply.push_back(std::byte{static_cast<std::uint8_t>(c)});
    reply.push_back(std::byte{0}); // NUL

    return ctx_.send_packet(sid::kEnterChat,
                            std::span<const std::byte>{reply});
}

// ---------------------------------------------------------------------------
// InChannel state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_join_channel(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_JOINCHANNEL out of order");
    }

    // SID_JOINCHANNEL (0x0C) body:
    //   [0..3]  flags        (LE uint32: 0=first join, 1=forced, 2=diablo2)
    //   [4..]   channel_name (NUL-terminated)

    // No reply required for JOINCHANNEL itself; the server sends
    // SID_CHATEVENT (0x0F) events to populate the channel.
    // Phase 5 will wire in the JoinChannel use-case.
    (void)payload;
    return core::ok();
}

core::Status<> ConnectionFsm::on_chat_command(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_CHATCOMMAND out of order");
    }

    // SID_CHATCOMMAND (0x0E) body:
    //   [0..]  text  (NUL-terminated)

    // Phase 5 will wire in the PostMessage / command dispatch use-cases.
    (void)payload;
    return core::ok();
}

core::Status<> ConnectionFsm::on_leave_channel(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        // Silently ignore if not in a channel
        return core::ok();
    }

    (void)payload;
    state_ = ConnectionState::LoggedIn;
    return core::ok();
}

core::Status<> ConnectionFsm::on_start_game(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_STARTADVEX out of order");
    }

    // SID_STARTADVEX (0x1C) / SID_STARTADVEX3 (0x1F) body layout:
    //   [0..3]   game_state   (LE uint32: 0=private, 1=public, 2=protected)
    //   [4..7]   game_type    (LE uint32: maps to GameType enum)
    //   [8..11]  unknown      (LE uint32)
    //   [12..15] ladder_type  (LE uint32)
    //   [16..]   game_name    (NUL-terminated)
    //   [..]     game_password(NUL-terminated)
    //   [..]     game_stats   (NUL-terminated)

    GameInfo info;
    const std::uint32_t raw_type = read_le32(payload, 4);
    switch (raw_type) {
        case 1:  info.game_type = GameType::FreeForAll;  break;
        case 2:  info.game_type = GameType::OneOnOne;    break;
        case 3:  info.game_type = GameType::Cooperative; break;
        case 4:  info.game_type = GameType::Custom;      break;
        default: info.game_type = GameType::Melee;       break;
    }

    // Parse variable-length strings starting at offset 16
    info.game_name = read_cstring(payload, 16);
    const std::size_t pw_offset = 16 + info.game_name.size() + 1;
    info.password  = read_cstring(payload, pw_offset);
    const std::size_t stats_offset = pw_offset + info.password.size() + 1;
    info.game_stats = read_cstring(payload, stats_offset);

    // Assign a new game ID and transition to InGame
    game_id_ = next_game_id_++;
    state_   = ConnectionState::InGame;

    // Notify the context
    ctx_.on_game_created(game_id_, info);

    // Reply: SID_STARTADVEX / SID_STARTADVEX3
    // Body: [0..3] result (0 = success)
    std::vector<std::byte> reply;
    write_le32(reply, 0u); // success

    return ctx_.send_packet(sid::kStartGame1,
                            std::span<const std::byte>{reply});
}

core::Status<> ConnectionFsm::on_join_game(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_GETADVLISTEX out of order");
    }

    // SID_GETADVLISTEX (0x09) body layout:
    //   [0..3]   game_type    (LE uint32)
    //   [4..7]   sub_game_type(LE uint32)
    //   [8..11]  language_id  (LE uint32)
    //   [12..15] ladder_type  (LE uint32)
    //   [16..19] num_results  (LE uint32)
    //   [20..]   game_name    (NUL-terminated)
    //   [..]     game_password(NUL-terminated)
    //   [..]     game_stats   (NUL-terminated)

    GameInfo info;
    const std::uint32_t raw_type = read_le32(payload, 0);
    switch (raw_type) {
        case 1:  info.game_type = GameType::FreeForAll;  break;
        case 2:  info.game_type = GameType::OneOnOne;    break;
        case 3:  info.game_type = GameType::Cooperative; break;
        case 4:  info.game_type = GameType::Custom;      break;
        default: info.game_type = GameType::Melee;       break;
    }

    // Parse variable-length strings starting at offset 20
    info.game_name = read_cstring(payload, 20);
    const std::size_t pw_offset = 20 + info.game_name.size() + 1;
    info.password  = read_cstring(payload, pw_offset);
    const std::size_t stats_offset = pw_offset + info.password.size() + 1;
    info.game_stats = read_cstring(payload, stats_offset);

    // Assign a new game ID and transition to InGame
    game_id_ = next_game_id_++;
    state_   = ConnectionState::InGame;

    // Notify the context
    ctx_.on_game_joined(game_id_, info);

    // Reply: SID_GETADVLISTEX (0x09)
    // Body: [0..3] result (0 = success / game found)
    std::vector<std::byte> reply;
    write_le32(reply, 0u); // success

    return ctx_.send_packet(sid::kJoinGame,
                            std::span<const std::byte>{reply});
}

// ---------------------------------------------------------------------------
// InGame state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_leave_game(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InGame) {
        // Silently ignore if not in a game (some clients send STOPADV spuriously)
        return core::ok();
    }

    (void)payload;

    const std::uint32_t left_game_id = game_id_;
    game_id_ = 0u;
    state_   = ConnectionState::InChannel;

    // Notify the context
    ctx_.on_game_left(left_game_id);

    return core::ok();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::reject(const char* reason) {
    state_ = ConnectionState::Disconnecting;
    ctx_.close();
    return core::fail(core::Error{core::StatusCode::InvalidArgument, reason});
}

core::Status<> ConnectionFsm::send_empty_reply(std::uint8_t packet_id) {
    return ctx_.send_packet(packet_id, std::span<const std::byte>{});
}

core::Status<> ConnectionFsm::send_result_reply(std::uint8_t packet_id,
                                                  std::uint32_t result) {
    std::vector<std::byte> body;
    write_le32(body, result);
    return ctx_.send_packet(packet_id, std::span<const std::byte>{body});
}

}  // namespace pvpgn::domain::connection
