// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file nls_crypto_adapter.hpp
/// Concrete `application::auth::INlsCryptoService` backed by the in-tree
/// `infra::crypto::NlsServer` implementation. Lives in the infra layer
/// so the application layer never reaches into infra/crypto directly.

#include "application/auth/nls_crypto.hpp"

namespace pvpgn::infra::crypto {

class NlsCryptoAdapter final : public application::auth::INlsCryptoService {
public:
    [[nodiscard]] application::auth::NlsCryptoContext create_challenge(
        std::string_view           username,
        std::span<const std::byte> verifier,
        std::span<const std::byte> stored_salt) override;

    [[nodiscard]] core::Result<std::array<std::byte, 20>,
                               application::auth::NlsCryptoError>
    verify_proof(application::auth::NlsCryptoContext& ctx,
                 std::string_view                     username,
                 std::span<const std::byte>           client_public_key_A,
                 std::span<const std::byte>           client_proof_M1) override;
};

}  // namespace pvpgn::infra::crypto
