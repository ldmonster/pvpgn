// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm_auth.cpp
/// BnetFsm — authentication-state handlers.
///
/// Covers the Init → AuthInfoReceived → LoggedIn state transitions:
///   on(AuthInfo)           — SID_AUTH_INFO (0x50)
///   on(LogonResponse2)     — SID_LOGONRESPONSE2 (0x3A) — OLS login
///   on(AuthCheckRequest)   — SID_AUTH_CHECK (0x51)
///   on(CdKey2Request)      — SID_CDKEY2 (0x36)
///   on(FileInfoRequest)    — SID_GETFILETIME (0x33)
///   on(LoginW3Request)     — SID_AUTH_ACCOUNTLOGON (0x53) — NLS step A
///   on(LogonProofW3Request)— SID_AUTH_ACCOUNTLOGONPROOF (0x54) — NLS step B
///   on(PassChangeRequest)  — SID_AUTH_ACCOUNTCHANGE (0x55)
///   on(PassChangeProofRequest) — SID_AUTH_ACCOUNTCHANGEPROOF (0x56)
///   on(CreateAccount2Request)  — SID_CREATEACCOUNT2 (0x3D)
///   Legacy OLS handlers (CompInfo1, ProgIdent, AuthReq1, CountryInfo1,
///                         CompInfo2, LoginReq1, CreateAccount1, Unknown2B,
///                         CdKeyLegacy, ChangePassword, Unknown39,
///                         CreateAccount, NetGamePort)

#include "protocol/bnet/fsm.hpp"

#include <array>
#include <cstdint>
#include <string>

#include "application/auth/create_account.hpp"
#include "application/auth/login_user.hpp"
#include "application/moderation/check_ip_ban.hpp"
#include "domain/identity/ports.hpp"
#include "core/error.hpp"

namespace pvpgn::protocol::bnet {

namespace {

/// Pack the 5×u32 OLS "hash1" words into their 20-byte little-endian wire
/// representation — exactly the form `domain::BNHash::from_bytes` expects
/// (it requires precisely 20 bytes). The same packing is used by the login
/// (SID_LOGONRESPONSE2) and create-account (SID_CREATEACCTREQ1) paths so a
/// password created on one round-trips to a successful login on the other.
std::string pack_hash1_le(const std::array<std::uint32_t, 5>& words) {
    std::string out(20, '\0');
    for (std::size_t i = 0; i < words.size(); ++i) {
        const std::uint32_t w = words[i];
        out[i * 4 + 0] = static_cast<char>(w & 0xFFu);
        out[i * 4 + 1] = static_cast<char>((w >> 8) & 0xFFu);
        out[i * 4 + 2] = static_cast<char>((w >> 16) & 0xFFu);
        out[i * 4 + 3] = static_cast<char>((w >> 24) & 0xFFu);
    }
    return out;
}

}  // namespace

core::Status<> BnetFsm::on(const AuthInfo& m) {
    if (state_ != BnetState::Init) {
        return reject("bnet fsm: AUTH_INFO out of order");
    }
    // Store the client product tag (game_id is the 4-byte product tag in
    // wire-packed big-endian form, e.g. 'STAR', 'D2DV', 'WAR3').
    if (auto tag = domain::ClientTag::from_packed_be(m.game_id)) {
        client_tag_ = tag.value();
    }
    state_ = BnetState::AuthInfoReceived;
    // Phase-5 use-case will plug version-check policy here. For now we
    // ack with result=0 ("passed") + empty info so a basic client can
    // proceed and exercise downstream states.
    return ctx_->send(ServerMessage{AuthCheckReply{0u, ""}});
}

core::Status<> BnetFsm::on(const LogonResponse2& m) {
    if (state_ != BnetState::AuthInfoReceived) {
        return reject("bnet fsm: LOGONRESPONSE2 out of order");
    }

    // Check IP ban if use-case is available
    if (use_cases_.check_ip_ban) {
        // TODO: peer_ip_ should be stored in constructor; for now, use placeholder 0
        // In production, this would be: auto ban_result = use_cases_.check_ip_ban->execute(peer_ip_);
    }

    if (m.username.empty()) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x01u, ""}});
    }

    // Call login use-case if available, otherwise accept the login
    if (!use_cases_.login_user) {
        // No login use-case available - accept login with default account ID
        current_username_ = std::string{m.username};
        state_ = BnetState::LoggedIn;
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x00u, ""}});
    }

    // Pack the 5×u32 hash1 words into their 20-byte LE wire form for BNHash.
    std::string password_hash = pack_hash1_le(m.password_hash);

    // Create login request with parsed credentials
    auto username_result = domain::UserName::parse(m.username);
    if (!username_result) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x01u, "Invalid username"}});
    }

    auto password_hash_result = domain::BNHash::from_bytes(password_hash);
    if (!password_hash_result) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x02u, "Invalid password hash"}});
    }

    application::auth::LoginRequest login_req{
        .name               = username_result.value(),
        .password_candidate = password_hash_result.value(),
        .tag                = client_tag_,
        .ip                 = domain::IpAddress{},
        // LoginUser enforces the single-session policy by attaching this
        // session itself — pass the real id (not a default 0) so the right
        // session is registered and we do NOT attach again below.
        .session            = session_id_
    };

    auto login_result = use_cases_.login_user->execute(login_req);
    if (!login_result) {
        // Login failed
        uint32_t    error_code = 0x02;  // default to bad password
        std::string reason;

        switch (login_result.error()) {
            case application::auth::LoginError::UnknownUser:
                error_code = 0x01;
                break;
            case application::auth::LoginError::InvalidCredentials:
                error_code = 0x02;
                break;
            case application::auth::LoginError::Locked:
                error_code = 0x05;
                reason     = "Account is locked";
                break;
            case application::auth::LoginError::Banned:
                error_code = 0x06;
                reason     = "Account has been banned";
                break;
            case application::auth::LoginError::MustChangePassword:
                error_code = 0x07;
                reason     = "Password must be changed";
                break;
            default:
                error_code = 0x02;
                break;
        }

        return ctx_->send(ServerMessage{LogonResponse2Reply{error_code, reason}});
    }

    // Login succeeded — store account ID and username. The session was
    // already attached by LoginUser (single-session policy); attaching again
    // here would fail with "account already has a session" and wrongly close
    // a valid login.
    current_account_id_ = login_result.value().id;
    current_username_   = std::string{m.username};

    state_ = BnetState::LoggedIn;
    return ctx_->send(ServerMessage{LogonResponse2Reply{0x00u, ""}});
}

core::Status<> BnetFsm::on(const AuthCheckRequest&) {
    if (state_ != BnetState::AuthInfoReceived &&
        state_ != BnetState::Init) {
        return reject("bnet fsm: AUTH_CHECK out of order");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const CdKey2Request&) {
    // CDKEY2 is the LoD second-key proof; legal once AUTH_INFO has
    // been exchanged and before the user is logged in.
    if (state_ != BnetState::AuthInfoReceived &&
        state_ != BnetState::Init) {
        return reject("bnet fsm: CDKEY2 out of order");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const FileInfoRequest&) {
    // GETFILETIME may be sent from AuthInfoReceived onwards (e.g. for
    // gateways/icons probes) — accept in any non-Closing state.
    return core::ok();
}

// NLS (SRP-6a) handlers — pre-login, accept in any state.
core::Status<> BnetFsm::on(const LoginW3Request&) {
    // NLS step A; pre-login. Accept in any state.
    return core::ok();
}
core::Status<> BnetFsm::on(const LogonProofW3Request&) {
    // NLS step B; pre-login. Accept in any state.
    return core::ok();
}
core::Status<> BnetFsm::on(const PassChangeRequest&) {
    // NLS password-change step A; runs before the user is fully logged in.
    return core::ok();
}
core::Status<> BnetFsm::on(const PassChangeProofRequest&) {
    // NLS password-change step B; runs before the user is fully logged in.
    return core::ok();
}
core::Status<> BnetFsm::on(const CreateAccount2Request&) {
    // CREATEACCOUNT2 is part of pre-login NLS account provisioning; accept advisorily.
    return core::ok();
}

// Legacy / OLS handlers: accept as advisory pre-login messages.
core::Status<> BnetFsm::on(const CompInfo1Request&)      { return core::ok(); }
core::Status<> BnetFsm::on(const ProgIdent&)             { return core::ok(); }
core::Status<> BnetFsm::on(const AuthReq1&)              { return core::ok(); }
core::Status<> BnetFsm::on(const CountryInfo1&)          { return core::ok(); }
core::Status<> BnetFsm::on(const CompInfo2&)             { return core::ok(); }
core::Status<> BnetFsm::on(const LoginReq1&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CreateAccount1Request& m) {
    // Legacy OLS account creation (SID_CREATEACCTREQ1). Permitted after the
    // version-check handshake, mirroring the LOGONRESPONSE2 ordering.
    if (state_ != BnetState::AuthInfoReceived) {
        return reject("bnet fsm: CREATEACCTREQ1 out of order");
    }
    // No create-account use-case wired → cannot persist; refuse honestly
    // rather than ACK a creation that did not happen.
    if (!use_cases_.create_account) {
        return ctx_->send(ServerMessage{CreateAccount1Reply{kCreateAccount1ResultNo}});
    }

    auto username = domain::UserName::parse(m.player_name);
    if (!username) {
        return ctx_->send(ServerMessage{CreateAccount1Reply{kCreateAccount1ResultNo}});
    }
    // Same 20-byte LE packing as login, so a created password logs in cleanly.
    auto password = domain::BNHash::from_bytes(pack_hash1_le(m.password_hash1));
    if (!password) {
        return ctx_->send(ServerMessage{CreateAccount1Reply{kCreateAccount1ResultNo}});
    }

    application::auth::CreateAccountRequest req{
        .username      = username.value(),
        .password_hash = password.value(),
        .email         = "",
        .locale        = domain::Locale{},
        .client_tag    = client_tag_,
        .peer_ip       = domain::IpAddress{},
    };
    auto result = use_cases_.create_account->execute(req);
    const std::uint32_t code =
        result ? kCreateAccount1ResultOk : kCreateAccount1ResultNo;
    return ctx_->send(ServerMessage{CreateAccount1Reply{code}});
}
core::Status<> BnetFsm::on(const Unknown2B&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CdKeyLegacyRequest&)    { return core::ok(); }
core::Status<> BnetFsm::on(const ChangePasswordRequest&) { return core::ok(); }
core::Status<> BnetFsm::on(const Unknown39&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CreateAccountRequest&)  { return core::ok(); }
core::Status<> BnetFsm::on(const NetGamePort&)           { return core::ok(); }

}  // namespace pvpgn::protocol::bnet
