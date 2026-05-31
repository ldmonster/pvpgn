// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_authenticating.cpp
/// ConnectionFsm — Authenticating-state handlers (NLS / SRP-6a).
///
/// Handlers in this TU:
///   on_auth_accountlogon()      — SID_AUTH_ACCOUNTLOGON (0x53): NLS SRP step 1
///   on_auth_accountlogonproof() — SID_AUTH_ACCOUNTLOGONPROOF (0x54): NLS SRP step 2

#include "application/connection/connection_fsm.hpp"

#include <array>
#include <cstring>
#include <span>
#include <vector>

#include "application/auth/login_user_nls.hpp"
#include "core/error.hpp"

#include "connection_fsm_internal.hpp"

namespace pvpgn::application::connection {

using namespace detail;

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
    username_ = pending_nls_username_.value_or(std::string{});
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

}  // namespace pvpgn::application::connection
