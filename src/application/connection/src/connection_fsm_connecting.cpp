// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_connecting.cpp
/// ConnectionFsm — Connecting-state handlers.
///
/// Handlers in this TU:
///   on_auth_info()      — SID_AUTH_INFO (0x50): product/version announcement
///   on_auth_check()     — SID_AUTH_CHECK (0x51): version check result
///   on_logon_request()  — SID_LOGON_REQUEST (0x29) / SID_LOGONRESPONSE2 (0x3A): OLS login

#include "application/connection/connection_fsm.hpp"

#include <array>
#include <cstring>
#include <span>
#include <vector>

#include "application/auth/login_user.hpp"
#include "core/error.hpp"

#include "connection_fsm_internal.hpp"

namespace pvpgn::application::connection {

using namespace detail;

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

    // For now: accept all version checks (real policy is a future addition).
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

    // WAR3/W3XP clients must use the NLS path (SID 0x53/0x54).
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

}  // namespace pvpgn::application::connection
