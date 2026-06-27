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
#include "protocol/bnet/account_wire_types.hpp"

#include <array>
#include <cstdint>
#include <string>

#include "application/auth/change_password.hpp"
#include "application/auth/create_account.hpp"
#include "application/auth/login_user.hpp"
#include "application/auth/login_user_w3.hpp"
#include "application/auth/srp3_credential_store.hpp"
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
    // Remember the client version id; the W3 NLS proof step uses it to decide
    // whether to prompt for an e-mail address (parity with the original's
    // `versionid >= 0x0D` gate).
    version_id_ = m.version_id;
    state_ = BnetState::AuthInfoReceived;

    // Reply with SID_AUTH_INFO (0x50) — the server seed — exactly as the
    // original does (handle_bnet.cpp SERVER_AUTHREQ_109). A real client BLOCKS
    // here waiting for this packet: it needs the server token (folded into the
    // password double-hash) and the logon-type flag (which selects the OLS vs
    // W3/NLS login path). Emitting the AUTH_CHECK result directly, as we used
    // to, left every real client stalled. The follow-up SID_AUTH_CHECK (0x51)
    // from the client is acked in on(AuthCheckRequest).
    AuthInfoReply reply;
    // logon-type: 2 for WarCraft III (WAR3/W3XP) which use the NLS/SRP login;
    // 0 (standard OLS broken-SHA-1) for every other Blizzard client.
    reply.logontype = (client_tag_ == domain::tags::kWarcraft3 ||
                       client_tag_ == domain::tags::kWar3Xp)
                          ? 2u
                          : 0u;
    // Per-session, always-nonzero server token (the client folds it into hash2
    // and echoes it back in SID_LOGONRESPONSE2). Odd by construction so it can
    // never collide with the 0 "no token" value a stale client might send.
    reply.server_token =
        (static_cast<std::uint32_t>(session_id_.value()) * 2654435761u) | 1u;
    server_token_ = reply.server_token;
    reply.session_num = 0u;
    reply.timestamp   = 0u;
    // Version-check is not enforced (parity with allow_unknown_version): send a
    // standard MPQ name + a representative CheckRevision equation so the shape
    // matches the original. The values are advisory — neither server validates
    // the returned checksum under the test config.
    reply.mpq_filename     = "ver-IX86-1.mpq";
    reply.checksum_formula = "A=1 B=1 C=1 4 A=A^S B=B^C C=C^A A=A^B";
    return ctx_->send(ServerMessage{reply});
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

    // `m.password_hash` is the client's hash2 (the double-hash
    // `bnet_hash(client_token ‖ server_token ‖ stored_hash1)`), NOT hash1. The
    // server recomputes the same double-hash from the stored hash1 + the tokens
    // (via the injected IPasswordHasher) and compares — so we route through the
    // session-hash overload, passing the tokens, rather than comparing the
    // client's hash2 against the stored hash1 directly.
    auto password_hash2_result = domain::BNHash::from_bytes(password_hash);
    if (!password_hash2_result) {
        return ctx_->send(ServerMessage{LogonResponse2Reply{0x02u, "Invalid password hash"}});
    }

    application::auth::LoginWithSessionHashRequest login_req{
        .name           = username_result.value(),
        .password_hash2 = password_hash2_result.value(),
        .ticks          = m.client_token,   // original `ticks`
        .sessionkey     = m.server_token,   // original `sessionkey`
        .tag            = client_tag_,
        .ip             = domain::IpAddress{},
        // LoginUser enforces the single-session policy by attaching this
        // session itself — pass the real id (not a default 0) so the right
        // session is registered and we do NOT attach again below.
        .session        = session_id_
    };

    auto login_result = use_cases_.login_user->execute(login_req);
    if (!login_result) {
        // Login failed. The OLS SID_LOGONRESPONSE2 reply only defines four
        // result codes (success/nonexist/badpass/locked, per bnet_protocol.h);
        // the original maps every access-denied reason that isn't "no such
        // account" or "wrong password" onto the LOCKED code (0x06) with an
        // explanatory message string — barred, already-logged-in, and locked
        // all share it. Anything else degrades to bad-password.
        uint32_t    error_code = account::kLoginReply2MessageBadPass;
        std::string reason;

        switch (login_result.error()) {
            case application::auth::LoginError::UnknownUser:
                error_code = account::kLoginReply2MessageNonExist;
                break;
            case application::auth::LoginError::InvalidCredentials:
                error_code = account::kLoginReply2MessageBadPass;
                break;
            case application::auth::LoginError::Locked:
                error_code = account::kLoginReply2MessageLocked;
                reason     = "Account is locked";
                break;
            case application::auth::LoginError::Banned:
                error_code = account::kLoginReply2MessageLocked;
                reason     = "Account has been banned";
                break;
            case application::auth::LoginError::MustChangePassword:
                error_code = account::kLoginReply2MessageBadPass;
                reason     = "Password must be changed";
                break;
            default:
                error_code = account::kLoginReply2MessageBadPass;
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

    // kick-old-login: if this login displaced a previously-online session for
    // the account, close that old connection (LoginUser already detached it).
    if (login_result.value().kicked_session && use_cases_.message_router) {
        (void)use_cases_.message_router->disconnect(
            login_result.value().kicked_session.value());
    }

    state_ = BnetState::LoggedIn;
    // Watch/presence: notify mutual online friends that we entered (mirrors the
    // original's conn_set_account -> WatchComponent::dispatch_whisper, ET_login).
    notify_friends_presence(/*entered=*/true);
    return ctx_->send(ServerMessage{LogonResponse2Reply{0x00u, ""}});
}

core::Status<> BnetFsm::on(const AuthCheckRequest&) {
    if (state_ != BnetState::AuthInfoReceived &&
        state_ != BnetState::Init) {
        return reject("bnet fsm: AUTH_CHECK out of order");
    }
    // Ack the version/CD-key check as passed (result=0). Version-check policy
    // is not enforced under the current config; the real client requires this
    // SID_AUTH_CHECK (0x51) reply before it will send SID_LOGONRESPONSE2.
    return ctx_->send(ServerMessage{AuthCheckReply{0u, ""}});
}

core::Status<> BnetFsm::on(const CdKey2Request& m) {
    // CDKEY2 is the LoD second-key proof; legal once AUTH_INFO has
    // been exchanged and before the user is logged in.
    if (state_ != BnetState::AuthInfoReceived &&
        state_ != BnetState::Init) {
        return reject("bnet fsm: CDKEY2 out of order");
    }
    // The original (_client_cdkey2) ALWAYS answers with SERVER_CDKEYREPLY2:
    // result = SERVER_CDKEYREPLY2_MESSAGE_OK (0x01) and the owner string echoed
    // back from the request. The real client waits for this reply before it
    // continues the login flow, so a silent accept would hang it.
    return ctx_->send(ServerMessage{CdKey2Reply{
        /*result*/ 1u,  // SERVER_CDKEYREPLY2_MESSAGE_OK
        /*owner*/  m.owner}});
}

core::Status<> BnetFsm::on(const FileInfoRequest& m) {
    // SID_GETFILETIME (0x33): the original (_client_fileinforeq) ALWAYS replies
    // with SERVER_FILEINFOREPLY echoing the request's type + unknown2 and the
    // requested filename, plus the file's server-side mtime (so the client can
    // decide whether to BNFTP-download a newer copy). v3 does not track these
    // files' mtimes at the protocol layer, so the timestamp is sent as 0 (a
    // placeholder); the echoed type/unknown2/filename match the oracle.
    return ctx_->send(ServerMessage{FileInfoReply{
        /*type*/      m.type,
        /*unknown2*/  m.unknown2,
        /*timestamp*/ 0,
        /*filename*/  m.filename}});
}

// WarCraft III SRP-3 (NLS) login — SID_AUTH_ACCOUNTLOGON (0x53) step A.
core::Status<> BnetFsm::on(const LoginW3Request& m) {
    // Permitted after the version-check handshake, like LOGONRESPONSE2.
    if (state_ != BnetState::AuthInfoReceived && state_ != BnetState::Init) {
        return reject("bnet fsm: LOGINREQ_W3 out of order");
    }
    w3_challenge_ready_ = false;

    // No W3 login use-case wired → report a bad account (cannot challenge).
    if (!use_cases_.login_user_w3) {
        return ctx_->send(ServerMessage{LoginW3Reply{kLoginW3MessageFailure, {}, {}}});
    }

    auto challenge = use_cases_.login_user_w3->challenge(
        m.account_name,
        std::span<const std::uint8_t, 32>{m.client_public_key});
    if (!challenge) {
        // No such account / no SRP-3 credentials.
        return ctx_->send(ServerMessage{LoginW3Reply{kLoginW3MessageFailure, {}, {}}});
    }

    const auto& c = challenge.value();
    w3_expected_m1_     = c.expected_client_proof;
    w3_server_m2_       = c.server_proof;
    w3_pending_account_ = c.account_id;
    w3_pending_username_ = m.account_name;
    w3_challenge_ready_ = true;

    return ctx_->send(ServerMessage{
        LoginW3Reply{kLoginW3MessageSuccess, c.salt, c.server_public_key}});
}

// SID_AUTH_ACCOUNTLOGONPROOF (0x54) step B — verify M1, return M2, log in.
core::Status<> BnetFsm::on(const LogonProofW3Request& m) {
    if (!w3_challenge_ready_) {
        // 0x54 before a successful 0x53 challenge.
        return ctx_->send(ServerMessage{
            LogonProofW3Reply{kLogonProofW3ResponseBadPass, {}, ""}});
    }
    w3_challenge_ready_ = false;  // single-shot; force a fresh challenge on retry

    if (m.client_password_proof != w3_expected_m1_) {
        return ctx_->send(ServerMessage{
            LogonProofW3Reply{kLogonProofW3ResponseBadPass, {}, ""}});
    }

    // Proof matched — attach the session (W3 bypasses LoginUser, so do it here)
    // and transition to LoggedIn before returning the server proof M2.
    // kick-old-login (the original's default): if the account already has a live
    // session, detach it and close that old connection, then attach this one.
    // (Previously the attach failure was ignored, leaving the new session
    // unregistered — a ghost — and the old session in place.)
    if (use_cases_.session_registry) {
        if (auto old =
                use_cases_.session_registry->session_for(w3_pending_account_)) {
            use_cases_.session_registry->detach(old.value());
            if (use_cases_.message_router) {
                (void)use_cases_.message_router->disconnect(old.value());
            }
        }
        (void)use_cases_.session_registry->attach(session_id_, w3_pending_account_);
    }
    current_account_id_ = w3_pending_account_;
    current_username_   = w3_pending_username_;
    state_              = BnetState::LoggedIn;
    // Watch/presence: notify mutual online friends that we entered (mirrors the
    // original's conn_set_account -> WatchComponent::dispatch_whisper, ET_login).
    notify_friends_presence(/*entered=*/true);

    // Mirror the original (_client_loginproofw3): once the proof checks out, a
    // client at version id >= 0x0D whose account has no e-mail on file is asked
    // to supply one — the proof reply carries RESPONSE_EMAIL (0x0E) instead of
    // RESPONSE_OK (0x00). It is still a successful login (the server proof M2 is
    // returned either way); the code only tells the client to pop the
    // "register e-mail" dialog.
    //
    // The core `Account` aggregate carries no e-mail address, W3 accounts are
    // created with an empty e-mail, and v3 implements no SETEMAIL flow — so a
    // W3 account never has an e-mail on file. The gate therefore reduces to the
    // version check. (If account e-mail becomes part of the aggregate, replace
    // the `false` below with the real lookup.)
    const bool has_email = false;
    const std::uint32_t response = (version_id_ >= 0x0Du && !has_email)
                                       ? kLogonProofW3ResponseEmail
                                       : kLogonProofW3ResponseOk;
    return ctx_->send(ServerMessage{
        LogonProofW3Reply{response, w3_server_m2_, ""}});
}

// SID_AUTH_ACCOUNTCHANGE (0x55) — NLS password-change step A. Identical SRP-3
// challenge to login (0x53): prove knowledge of the CURRENT password before the
// new salt/verifier (carried in the 0x56 proof) are accepted. Mirrors the
// original `_client_passchangereq` (a copy of `_client_loginreqw3`).
core::Status<> BnetFsm::on(const PassChangeRequest& m) {
    if (state_ != BnetState::AuthInfoReceived && state_ != BnetState::Init) {
        return reject("bnet fsm: PASSCHANGEREQ out of order");
    }
    w3_challenge_ready_ = false;

    if (!use_cases_.login_user_w3) {
        return ctx_->send(ServerMessage{
            PassChangeReply{kPassChangeMessageReject, {}, {}}});
    }
    auto challenge = use_cases_.login_user_w3->challenge(
        m.account_name,
        std::span<const std::uint8_t, 32>{m.client_public_key});
    if (!challenge) {
        return ctx_->send(ServerMessage{
            PassChangeReply{kPassChangeMessageReject, {}, {}}});
    }
    const auto& c = challenge.value();
    w3_expected_m1_      = c.expected_client_proof;
    w3_server_m2_        = c.server_proof;
    w3_pending_account_  = c.account_id;
    w3_pending_username_ = m.account_name;
    w3_challenge_ready_  = true;
    return ctx_->send(ServerMessage{
        PassChangeReply{kPassChangeMessageAccept, c.salt, c.server_public_key}});
}

// SID_AUTH_ACCOUNTCHANGEPROOF (0x56) — NLS password-change step B. Verify the
// M1 proof of the CURRENT password; on success replace the stored salt+verifier
// with the new ones the client supplied, then return M2. Mirrors the original
// `_client_passchangeproofreq`. (Unlike login, there is no e-mail prompt here.)
core::Status<> BnetFsm::on(const PassChangeProofRequest& m) {
    if (!w3_challenge_ready_) {
        return ctx_->send(ServerMessage{
            PassChangeProofReply{kPassChangeProofResponseBadPass, {}}});
    }
    w3_challenge_ready_ = false;  // single-shot

    if (m.client_password_proof != w3_expected_m1_) {
        return ctx_->send(ServerMessage{
            PassChangeProofReply{kPassChangeProofResponseBadPass, {}}});
    }
    // Proof of the old password is valid — persist the new credentials. Without
    // a store we cannot honour the change, so report BadPass rather than ACK a
    // change that did not happen.
    if (!use_cases_.srp3_store) {
        return ctx_->send(ServerMessage{
            PassChangeProofReply{kPassChangeProofResponseBadPass, {}}});
    }
    application::auth::Srp3Credentials creds;
    creds.salt       = m.salt;
    creds.verifier   = m.password_verifier;
    creds.account_id = w3_pending_account_;
    use_cases_.srp3_store->store(w3_pending_username_, creds);

    return ctx_->send(ServerMessage{
        PassChangeProofReply{kPassChangeProofResponseOk, w3_server_m2_}});
}

// SID_AUTH_ACCOUNTCREATE (0x52) — WarCraft III SRP-3 account creation. The
// client supplies a 32-byte salt + 32-byte verifier; the server never sees the
// password. We create the account (with a placeholder OLS hash — W3 logs in via
// SRP, not OLS) and persist the salt/verifier in the SRP-3 credential store.
core::Status<> BnetFsm::on(const CreateAccount2Request& m) {
    if (state_ != BnetState::AuthInfoReceived && state_ != BnetState::Init) {
        return reject("bnet fsm: CREATEACCOUNT2 out of order");
    }
    if (!use_cases_.create_account || !use_cases_.srp3_store) {
        return ctx_->send(
            ServerMessage{CreateAccount2Reply{kCreateAccount2ResultExists}});
    }
    auto username = domain::UserName::parse(m.account_name);
    if (!username) {
        return ctx_->send(
            ServerMessage{CreateAccount2Reply{kCreateAccount2ResultInvalid}});
    }
    // W3 accounts have no OLS password; store a fixed placeholder hash.
    auto password = domain::BNHash::from_bytes(std::string(20, '\0'));
    if (!password) {
        return ctx_->send(
            ServerMessage{CreateAccount2Reply{kCreateAccount2ResultInvalid}});
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
    if (!result) {
        return ctx_->send(
            ServerMessage{CreateAccount2Reply{kCreateAccount2ResultExists}});
    }

    application::auth::Srp3Credentials creds;
    creds.salt       = m.salt;
    creds.verifier   = m.password_verifier;
    creds.account_id = result.value();
    use_cases_.srp3_store->store(m.account_name, creds);

    return ctx_->send(
        ServerMessage{CreateAccount2Reply{kCreateAccount2ResultOk}});
}

namespace {
// Deterministic, always-nonzero per-session value used as the legacy/OLS
// "session key". The oracle (conn_get_sessionkey) uses a random per-connection
// value; only the SID/length is wire-observable to peers, so a stable nonzero
// hash of the session id is faithful and reproducible. Distinct multiplier from
// server_token_ so the two values do not coincide.
std::uint32_t legacy_session_key(domain::SessionId id) {
    return (static_cast<std::uint32_t>(id.value()) * 2246822519u) | 1u;
}
}  // namespace

// CLIENT_COMPINFO1 (SID 0x05): first packet of the legacy/OLS retail login flow
// (Starcraft/Diablo pre-NLS). The oracle's _client_compinfo1 always replies with
// SERVER_COMPREPLY (the four magic registration constants) followed by
// SERVER_SESSIONKEY1 carrying the connection's session key. v3 used to drop both
// replies, stalling legacy clients — emit them to match.
core::Status<> BnetFsm::on(const CompInfo1Request&) {
    if (auto st = ctx_->send(ServerMessage{CompReply{}}); !st) {
        return st;
    }
    return ctx_->send(
        ServerMessage{SessionKey1{.sessionkey = legacy_session_key(session_id_)}});
}
// CLIENT_PROGIDENT (SID 0x06): the first packet of the legacy/pre-NLS login
// flow used by Diablo / old Starcraft / D2 clients. The oracle's
// _client_progident records the client identity and always replies with
// SERVER_AUTHREQ1 carrying the versioncheck filename + CheckRevision equation
// (select_checkrevision returns a hard-coded default when no versioncheck
// config is present). v3 used to drop the reply, stalling every legacy client
// that waits forever for the filename/equation it needs to proceed.
core::Status<> BnetFsm::on(const ProgIdent& m) {
    // Record the client identity (parity with conn_set_clienttag /
    // conn_set_versionid). game_id/clienttag is the 4-byte product tag in
    // wire-packed big-endian form (e.g. 'STAR', 'D2DV').
    if (auto tag = domain::ClientTag::from_packed_be(m.clienttag)) {
        client_tag_ = tag.value();
    }
    version_id_ = m.versionid;

    AuthReq1Server reply;
    reply.timestamp = 0u;  // advisory; oracle uses the versioncheck file mtime
    // Same fixed MPQ name + representative CheckRevision equation v3's
    // on(AuthInfo) already emits — values are advisory under the test config.
    reply.filename = "ver-IX86-1.mpq";
    reply.equation = "A=1 B=1 C=1 4 A=A^S B=B^C C=C^A A=A^B";
    return ctx_->send(ServerMessage{reply});
}
core::Status<> BnetFsm::on(const AuthReq1&)              { return core::ok(); }
core::Status<> BnetFsm::on(const CountryInfo1&)          { return core::ok(); }
// CLIENT_COMPINFO2 (SID 0x1E): sibling of COMPINFO1. The oracle replies with
// SERVER_COMPREPLY followed by SERVER_SESSIONKEY2 (sessionnum + sessionkey).
core::Status<> BnetFsm::on(const CompInfo2&) {
    if (auto st = ctx_->send(ServerMessage{CompReply{}}); !st) {
        return st;
    }
    return ctx_->send(ServerMessage{SessionKey2{
        .sessionnum = static_cast<std::uint32_t>(session_id_.value()),
        .sessionkey = legacy_session_key(session_id_)}});
}
core::Status<> BnetFsm::on(const LoginReq1&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CreateAccount1Request& m) {
    // Legacy OLS account creation (SID_CREATEACCTREQ1). Permitted after the
    // version-check handshake, mirroring the LOGONRESPONSE2 ordering.
    if (state_ != BnetState::AuthInfoReceived) {
        return reject("bnet fsm: CREATEACCTREQ1 out of order");
    }
    // Wire-field cap parity: the original reads the username with
    // packet_get_str_const(..., UNCHECKED_NAME_STR=32), which returns NULL once
    // the name exceeds 32 bytes → _client_createacctreq1 returns -1 →
    // conn_close_read: NO reply, connection dropped. Match that (rather than
    // sending a CREATEACCTREPLY1=NO with the connection left open).
    if (m.player_name.size() > 32) {
        return reject("bnet fsm: CREATEACCTREQ1 username too long");
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
core::Status<> BnetFsm::on(const ChangePasswordRequest& m) {
    // SID_CHANGEPASSWORD (0x31), legacy OLS: the client sends the old password
    // as a session-hash (double-hash of stored hash1 with ticks+sessionkey) and
    // the new password as hash1. The use-case re-derives via the same hasher and
    // rotates on a match. Mirrors the original _client_changepassreq; reply
    // SERVER_CHANGEPASSACK with success/fail.
    if (!use_cases_.change_password) {
        return ctx_->send(
            ServerMessage{ChangePasswordReply{kChangePasswordMessageFail}});
    }
    auto username = domain::UserName::parse(m.player_name);
    auto old_h2 = domain::BNHash::from_bytes(pack_hash1_le(m.oldpassword_hash2));
    auto new_h1 = domain::BNHash::from_bytes(pack_hash1_le(m.newpassword_hash1));
    if (!username || !old_h2 || !new_h1) {
        return ctx_->send(
            ServerMessage{ChangePasswordReply{kChangePasswordMessageFail}});
    }
    application::auth::ChangePasswordWithSessionHashRequest req{
        .name                   = username.value(),
        .current_password_hash2 = old_h2.value(),
        .ticks                  = m.ticks,
        .sessionkey             = m.sessionkey,
        .new_password           = new_h1.value(),
    };
    auto result = use_cases_.change_password->execute(req);
    const std::uint32_t code =
        result ? kChangePasswordMessageSuccess : kChangePasswordMessageFail;
    return ctx_->send(ServerMessage{ChangePasswordReply{code}});
}
core::Status<> BnetFsm::on(const Unknown39&)             { return core::ok(); }
core::Status<> BnetFsm::on(const CreateAccountRequest&)  { return core::ok(); }
core::Status<> BnetFsm::on(const NetGamePort&)           { return core::ok(); }

}  // namespace pvpgn::protocol::bnet
