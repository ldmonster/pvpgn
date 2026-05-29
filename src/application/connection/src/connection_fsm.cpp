// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/connection/connection_fsm.hpp"

#include <array>
#include <cstring>
#include <span>
#include <variant>
#include <vector>

#include "application/auth/login_user.hpp"
#include "application/auth/login_user_nls.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/post_message.hpp"
#include "core/bytes.hpp"
#include "core/error.hpp"

namespace pvpgn::application::connection {

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

        // --- D2 character select (R287) ---
        case sid::kD2CharSelect:
            return on_d2_char_select(payload);

        // --- WAR3 route token (R288) ---
        case sid::kWarcraftGeneral:
            return on_warcraft_general(payload);

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

    // R283: WAR3/W3XP clients must use the NLS path (SID 0x53/0x54).
    // Reject them here with an "invalid password" result so the client
    // knows the login failed rather than hanging.
    if (is_nls_client()) {
        std::vector<std::byte> reply;
        write_le32(reply, 0x01u); // result = invalid password / wrong auth method
        return ctx_.send_packet(sid::kLogonRequest,
                                std::span<const std::byte>{reply});
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

    // OLS path: call LoginUser::execute() if the use-case is wired in.
    if (login_user_ols_ != nullptr) {
        // Build a LoginRequest from the parsed OLS credentials.
        // The password hash occupies bytes [8..27] (5 × LE uint32).
        domain::BNHash::Bytes hash_bytes{};
        if (payload.size() >= 28) {
            std::memcpy(hash_bytes.data(), payload.data() + 8,
                        domain::BNHash::kSize);
        }

        auto name_result = domain::UserName::parse(uname);
        if (!name_result) {
            // Username failed validation — reject.
            std::vector<std::byte> reply;
            write_le32(reply, 0x01u); // invalid password / bad username
            return ctx_.send_packet(sid::kLogonRequest,
                                    std::span<const std::byte>{reply});
        }

        // ClientTag::from_packed_be() may fail for unknown/zero tags;
        // fall back to a default-constructed tag on parse failure.
        auto tag_result = domain::ClientTag::from_packed_be(client_product_tag_);
        domain::ClientTag tag = tag_result
            ? std::move(tag_result).value()
            : domain::ClientTag{};

        application::auth::LoginRequest req{
            std::move(name_result).value(),
            domain::BNHash{hash_bytes},
            std::move(tag),
            domain::IpAddress{},   // not available at this layer
            domain::SessionId{session_id_},
        };

        auto login_result = login_user_ols_->execute(std::move(req));
        if (!login_result) {
            // Authentication failed — send wire error result 0x01.
            std::vector<std::byte> reply;
            write_le32(reply, 0x01u); // invalid password
            return ctx_.send_packet(sid::kLogonRequest,
                                    std::span<const std::byte>{reply});
        }

        username_   = uname;
        account_id_ = login_result.value().id.value();
        state_      = ConnectionState::LoggedIn;

        std::vector<std::byte> reply;
        write_le32(reply, 0u); // success
        return ctx_.send_packet(sid::kLogonRequest,
                                std::span<const std::byte>{reply});
    }

    // Fallback: no LoginUser use-case injected — accept the legacy login
    // without credential verification (skeleton / test mode).
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

    // R283: Only WAR3/W3XP clients should reach this handler.
    // Non-NLS clients (STAR/SEXP/D2DV/D2XP) use SID_LOGON_REQUEST (0x29).
    if (!is_nls_client()) {
        // Unexpected NLS challenge from an OLS client — reject.
        std::vector<std::byte> reply;
        write_le32(reply, 0x01u); // result = account does not exist
        for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0}); // salt
        for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0}); // server_key
        return ctx_.send_packet(sid::kAuthAccountLogon,
                                std::span<const std::byte>{reply});
    }

    // Extract the 32-byte client public key A from [0..31].
    std::array<std::byte, 32> client_key_A{};
    const std::size_t key_bytes = std::min(payload.size(), std::size_t{32});
    std::memcpy(client_key_A.data(), payload.data(), key_bytes);

    // R283: If we have a LoginUserNls use-case, call challenge().
    if (login_user_nls_ != nullptr) {
        const core::ByteView key_view{client_key_A.data(), client_key_A.size()};
        auto result = login_user_nls_->challenge(uname, key_view);

        if (!result) {
            // Challenge failed (account not found or crypto error).
            // Map NlsLoginError to a wire result code.
            const auto err = result.error();
            const std::uint32_t wire_result =
                (err == application::auth::NlsLoginError::AccountNotFound) ? 0x01u : 0x02u;

            std::vector<std::byte> reply;
            write_le32(reply, wire_result);
            for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0}); // salt
            for (int i = 0; i < 32; ++i) reply.push_back(std::byte{0}); // server_key
            return ctx_.send_packet(sid::kAuthAccountLogon,
                                    std::span<const std::byte>{reply});
        }

        // Store per-session NLS state for the subsequent verify() call.
        auto& challenge_result   = result.value();
        pending_nls_ctx_         = std::move(challenge_result.crypto_ctx);
        pending_nls_username_    = uname;
        pending_nls_client_key_  = client_key_A;
        pending_nls_account_id_  = challenge_result.account_id;

        // Reply: SID_AUTH_ACCOUNTLOGON (0x53)
        // Body:
        //   [0..3]   result      (0 = success, account exists)
        //   [4..35]  salt        (32 bytes)
        //   [36..67] server_key  (32 bytes)
        std::vector<std::byte> reply;
        write_le32(reply, 0u); // result = success (account exists)
        for (const auto b : challenge_result.salt)
            reply.push_back(b);
        for (const auto b : challenge_result.server_public_key)
            reply.push_back(b);

        return ctx_.send_packet(sid::kAuthAccountLogon,
                                std::span<const std::byte>{reply});
    }

    // Fallback: no LoginUserNls injected — store username and send placeholder
    // zeros (skeleton behaviour, same as before R283 for this code path).
    pending_nls_username_   = uname;
    pending_nls_client_key_ = client_key_A;

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

    // R283: If we have a LoginUserNls use-case and stored NLS context,
    // call verify() to authenticate the client.
    if (login_user_nls_ != nullptr &&
        pending_nls_ctx_.has_value() &&
        pending_nls_username_.has_value() &&
        pending_nls_client_key_.has_value()) {

        // Extract the 20-byte client proof M1 from [0..19].
        std::array<std::byte, 20> client_proof_M1{};
        const std::size_t proof_bytes = std::min(payload.size(), std::size_t{20});
        std::memcpy(client_proof_M1.data(), payload.data(), proof_bytes);

        const core::ByteView key_view{pending_nls_client_key_->data(),
                                      pending_nls_client_key_->size()};
        const core::ByteView proof_view{client_proof_M1.data(),
                                        client_proof_M1.size()};

        // Capture username and account_id BEFORE clearing pending state.
        std::string captured_username = *pending_nls_username_;
        const domain::AccountId captured_account_id =
            pending_nls_account_id_.value_or(domain::AccountId{0});

        auto result = login_user_nls_->verify(
            captured_username,
            *pending_nls_ctx_,
            key_view,
            proof_view,
            captured_account_id);

        // Always clear pending NLS state regardless of outcome.
        clear_pending_nls();

        if (!result) {
            // Verification failed — send failure result, stay in Authenticating.
            // Wire result 0x02 = "incorrect password".
            std::vector<std::byte> reply;
            write_le32(reply, 0x02u); // incorrect password
            for (int i = 0; i < 20; ++i) reply.push_back(std::byte{0}); // server_proof (zeros)
            return ctx_.send_packet(sid::kAuthAccountLogonProof,
                                    std::span<const std::byte>{reply});
        }

        // Success: transition to LoggedIn.
        username_   = std::move(captured_username);
        account_id_ = result.value().account_id.value();
        state_      = ConnectionState::LoggedIn;

        // Reply: SID_AUTH_ACCOUNTLOGONPROOF (0x54)
        // Body:
        //   [0..3]   result        (0 = success)
        //   [4..23]  server_proof  (20 bytes, NLS SRP M2)
        std::vector<std::byte> reply;
        write_le32(reply, 0u); // success
        for (const auto b : result.value().server_proof)
            reply.push_back(b);

        return ctx_.send_packet(sid::kAuthAccountLogonProof,
                                std::span<const std::byte>{reply});
    }

    // Fallback: no LoginUserNls or no pending context — accept all proofs
    // (skeleton behaviour for OLS clients or when NLS use-case is absent).
    clear_pending_nls();
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

// ---------------------------------------------------------------------------
// Internal helper — encode and send a SID_CHATEVENT (0x0F) packet.
//
// SID_CHATEVENT wire layout (all LE):
//   uint32  event_id
//   uint32  user_flags
//   uint32  ping_ms
//   uint32  ip_address        (0x00000000 for server-generated events)
//   uint32  account_number    (0xBADC0FFE for server-generated events)
//   uint32  registration_auth (0xBADC0FFE for server-generated events)
//   char[]  username          (NUL-terminated)
//   char[]  text              (NUL-terminated)
// ---------------------------------------------------------------------------

namespace {

/// Server-side sentinel values for ip/account/registration fields.
inline constexpr std::uint32_t kServerIp           = 0x00000000u;
inline constexpr std::uint32_t kServerAcctNumber   = 0xBADC0FFEu;
inline constexpr std::uint32_t kServerRegAuth      = 0xBADC0FFEu;

/// SID_CHATEVENT event IDs (EID_* from legacy bnet_protocol.h).
inline constexpr std::uint32_t kEidShowUser  = 0x01u;  ///< EID_SHOWUSER  — existing member on join
inline constexpr std::uint32_t kEidJoin      = 0x02u;  ///< EID_JOIN      — member joined
inline constexpr std::uint32_t kEidLeave     = 0x03u;  ///< EID_LEAVE     — member left
inline constexpr std::uint32_t kEidTalk      = 0x05u;  ///< EID_TALK      — chat message
inline constexpr std::uint32_t kEidChannel   = 0x07u;  ///< EID_CHANNEL   — channel name notification
inline constexpr std::uint32_t kEidInfo      = 0x12u;  ///< EID_INFO      — informational text
inline constexpr std::uint32_t kEidError     = 0x13u;  ///< EID_ERROR     — error text
inline constexpr std::uint32_t kEidEmote     = 0x17u;  ///< EID_EMOTE     — /me emote

/// Build a raw SID_CHATEVENT body (without the 4-byte BNCS header).
/// The caller passes the body to ctx_.send_packet(0x0F, ...).
std::vector<std::byte> build_chat_event(std::uint32_t event_id,
                                         std::uint32_t flags,
                                         std::uint32_t ping_ms,
                                         std::string_view username,
                                         std::string_view text) {
    std::vector<std::byte> body;
    body.reserve(24 + username.size() + 1 + text.size() + 1);

    auto push_le32 = [&](std::uint32_t v) {
        body.push_back(std::byte{static_cast<std::uint8_t>( v        & 0xFFu)});
        body.push_back(std::byte{static_cast<std::uint8_t>((v >>  8) & 0xFFu)});
        body.push_back(std::byte{static_cast<std::uint8_t>((v >> 16) & 0xFFu)});
        body.push_back(std::byte{static_cast<std::uint8_t>((v >> 24) & 0xFFu)});
    };

    push_le32(event_id);
    push_le32(flags);
    push_le32(ping_ms);
    push_le32(kServerIp);
    push_le32(kServerAcctNumber);
    push_le32(kServerRegAuth);

    for (char c : username) body.push_back(std::byte{static_cast<std::uint8_t>(c)});
    body.push_back(std::byte{0}); // NUL

    for (char c : text) body.push_back(std::byte{static_cast<std::uint8_t>(c)});
    body.push_back(std::byte{0}); // NUL

    return body;
}

}  // namespace (anonymous, extended)

// ---------------------------------------------------------------------------
// R296 — InChannel state handlers (wired to real use-cases)
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_join_channel(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_JOINCHANNEL out of order");
    }

    // SID_JOINCHANNEL (0x0C) body:
    //   [0..3]  flags        (LE uint32: 0=first join, 1=forced, 2=diablo2)
    //   [4..]   channel_name (NUL-terminated)

    const std::string channel_name = read_cstring(payload, 4);
    if (channel_name.empty()) {
        // Ignore empty channel name — client bug or keepalive variant.
        return core::ok();
    }

    // If the JoinChannel use-case is not injected, fall back to stub behaviour:
    // record the channel name locally and send an EID_CHANNEL notification so
    // the client knows which channel it is in.
    if (join_channel_ == nullptr) {
        channel_name_ = channel_name;
        channel_id_   = 0u; // unknown without the use-case

        // Send EID_CHANNEL so the client UI updates its channel display.
        const auto body = build_chat_event(kEidChannel, 0u, 0u,
                                           channel_name_, "");
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    // Build a ClientTag from the stored product tag.
    auto tag_result = domain::ClientTag::from_packed_be(client_product_tag_);
    domain::ClientTag tag = tag_result
        ? std::move(tag_result).value()
        : domain::ClientTag{};

    // Execute the JoinChannel use-case.
    auto result = join_channel_->execute(
        domain::AccountId{account_id_},
        channel_name,
        tag);

    if (!result) {
        // Join failed — send an EID_ERROR to the client.
        const auto body = build_chat_event(kEidError, 0u, 0u,
                                           "", "Failed to join channel.");
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    // Success: store channel state.
    // Move the result out so we can call drain_events() (non-const).
    auto join_result = std::move(result).value();
    channel_id_   = join_result.channel.id().value();
    channel_name_ = join_result.channel.name();

    // 1. Send EID_CHANNEL so the client UI updates its channel display.
    {
        const auto body = build_chat_event(kEidChannel, 0u, 0u,
                                           channel_name_, "");
        if (auto s = ctx_.send_packet(0x0Fu, std::span<const std::byte>{body}); !s) {
            return s;
        }
    }

    // 2. Send EID_SHOWUSER for each existing member (so the client populates
    //    its user list before the EID_JOIN for the joining user).
    for (const auto& member_id : join_result.channel.member_ids()) {
        // We only have AccountId here; use account_id as username placeholder
        // until a full account-name lookup is wired in (Phase 6).
        const std::string member_name = std::to_string(member_id.value());
        const auto body = build_chat_event(kEidShowUser, 0u, 0u,
                                           member_name, "");
        if (auto s = ctx_.send_packet(0x0Fu, std::span<const std::byte>{body}); !s) {
            return s;
        }
    }

    // 3. Drain and dispatch domain events from the channel aggregate.
    //    The channel aggregate emits ChannelJoined for the joining user;
    //    we broadcast EID_JOIN to the other members via their sessions.
    //    For now we send EID_JOIN back to the joining client itself as well
    //    (legacy BNet behaviour: the server echoes the join to the joiner).
    const auto events = join_result.channel.drain_events();
    for (const auto& ev : events) {
        std::visit([&](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, domain::events::ChannelJoined>) {
                const std::string who_name = std::to_string(e.who.value());
                const auto body = build_chat_event(kEidJoin, 0u, 0u,
                                                   who_name, "");
                (void)ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
            }
        }, ev);
    }

    return core::ok();
}

core::Status<> ConnectionFsm::on_chat_command(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_CHATCOMMAND out of order");
    }

    // SID_CHATCOMMAND (0x0E) body:
    //   [0..]  text  (NUL-terminated)

    const std::string text = read_cstring(payload, 0);
    if (text.empty()) {
        return core::ok();
    }

    // If the text starts with '/', treat it as a command.
    // Full command dispatch is R298; for now just log it.
    if (text.front() == '/') {
        // TODO(R298): dispatch to command registry.
        // For now: silently acknowledge (no error sent to client).
        return core::ok();
    }

    // Regular chat message — call PostMessage use-case if injected.
    if (post_message_ == nullptr || channel_id_ == 0u) {
        // No use-case or not in a channel — stub: no-op.
        return core::ok();
    }

    // Validate and create a ChatMessage value object.
    auto msg_result = domain::ChatMessage::create(text);
    if (!msg_result) {
        // Message validation failed (too long, control chars, etc.) — ignore.
        return core::ok();
    }

    auto result = post_message_->execute(
        domain::ChannelId{channel_id_},
        domain::AccountId{account_id_},
        msg_result.value());

    if (!result) {
        // Post failed (not in channel, muted, etc.) — send EID_ERROR.
        const auto body = build_chat_event(kEidError, 0u, 0u,
                                           "", "Cannot send message.");
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    // Success: drain and dispatch the ChannelMessageSent event.
    // The PostMessage use-case returns the event + recipient session IDs.
    // For this connection we echo the message back as EID_TALK.
    const auto& post_result = result.value();
    const auto& msg_event   = post_result.event;

    const std::string sender_name = std::to_string(msg_event.from.value());
    const auto body = build_chat_event(kEidTalk, 0u, 0u,
                                       sender_name,
                                       std::string{msg_event.body.text()});
    return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
}

core::Status<> ConnectionFsm::on_leave_channel(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        // Silently ignore if not in a channel
        return core::ok();
    }

    (void)payload;

    // Call LeaveChannel use-case if injected and we have a valid channel.
    if (leave_channel_ != nullptr && channel_id_ != 0u) {
        (void)leave_channel_->execute(
            domain::ChannelId{channel_id_},
            domain::AccountId{account_id_});
        // Ignore errors — we are leaving regardless.
    }

    // Clear channel state.
    channel_id_   = 0u;
    channel_name_.clear();
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

void ConnectionFsm::clear_pending_nls() noexcept {
    pending_nls_ctx_.reset();
    pending_nls_username_.reset();
    pending_nls_client_key_.reset();
    pending_nls_account_id_.reset();
}

// ---------------------------------------------------------------------------
// R287 — D2 character select handler
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_d2_char_select(
    std::span<const std::byte> payload) {
    // SID_D2GAMELISTEX (0x68) body layout (D2 character-select variant):
    //   [0]      char_class  (uint8)
    //   [1]      char_level  (uint8)
    //   [2..]    char_name   (NUL-terminated)
    //
    // This packet is legal in LoggedIn, InChannel, and InGame states.
    // Silently ignore in Connecting / Authenticating / Disconnecting.
    if (state_ == ConnectionState::Connecting    ||
        state_ == ConnectionState::Authenticating ||
        state_ == ConnectionState::Disconnecting) {
        return core::ok();
    }

    if (payload.size() < 3) {
        // Too short to contain class + level + at least one name byte.
        return core::ok();
    }

    const std::uint8_t char_class = static_cast<std::uint8_t>(payload[0]);
    const std::uint8_t char_level = static_cast<std::uint8_t>(payload[1]);
    const std::string  char_name  = read_cstring(payload, 2);

    bind_d2_character(char_name, char_class, char_level);
    return core::ok();
}

// ---------------------------------------------------------------------------
// R288 — WAR3 route token handler
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_warcraft_general(
    std::span<const std::byte> payload) {
    // SID_WARCRAFTGENERAL (0x44) body layout:
    //   [0]      subcommand  (uint8)
    //   [1..4]   route_token (LE uint32) — present in WID_GAMESEARCH (0x00)
    //            and several other subcommands.
    //
    // We extract the token from any subcommand that carries it at [1..4].
    // The token is used by RouteRegistry to pair the primary and route
    // connections.  Silently ignore if the payload is too short.
    if (payload.size() < 5) {
        return core::ok();
    }

    // The route token is at bytes [1..4] (LE uint32) for the subcommands
    // that carry it (WID_GAMESEARCH = 0x00, WID_CANCELSEARCH = 0x01, etc.).
    const std::uint32_t token = read_le32(payload, 1);
    set_war3_route_token(token);
    return core::ok();
}

}  // namespace pvpgn::application::connection
