// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/crypto/nls_crypto_adapter.hpp"

#include "infra/crypto/nls.hpp"

namespace pvpgn::infra::crypto {

namespace {

/// Convert the application-layer port context into the concrete
/// infra::crypto::NlsContext. Field layout is identical by construction.
NlsContext to_infra(const application::auth::NlsCryptoContext& src) noexcept {
    NlsContext dst;
    dst.salt               = src.salt;
    dst.server_private_key = src.server_private_key;
    dst.server_public_key  = src.server_public_key;
    dst.session_key        = src.session_key;
    return dst;
}

application::auth::NlsCryptoContext to_port(const NlsContext& src) noexcept {
    application::auth::NlsCryptoContext dst;
    dst.salt               = src.salt;
    dst.server_private_key = src.server_private_key;
    dst.server_public_key  = src.server_public_key;
    dst.session_key        = src.session_key;
    return dst;
}

application::auth::NlsCryptoError map_error(NlsError e) noexcept {
    switch (e) {
        case NlsError::InvalidProof:     return application::auth::NlsCryptoError::InvalidProof;
        case NlsError::InvalidPublicKey: return application::auth::NlsCryptoError::InvalidPublicKey;
        case NlsError::CryptoError:      return application::auth::NlsCryptoError::CryptoError;
    }
    return application::auth::NlsCryptoError::CryptoError;
}

}  // namespace

application::auth::NlsCryptoContext NlsCryptoAdapter::create_challenge(
    std::string_view           username,
    std::span<const std::byte> verifier,
    std::span<const std::byte> stored_salt) {
    NlsContext ctx = NlsServer::create_challenge(username, verifier, stored_salt);
    return to_port(ctx);
}

core::Result<std::array<std::byte, 20>, application::auth::NlsCryptoError>
NlsCryptoAdapter::verify_proof(application::auth::NlsCryptoContext& ctx,
                               std::string_view                     username,
                               std::span<const std::byte>           client_public_key_A,
                               std::span<const std::byte>           client_proof_M1) {
    NlsContext mutable_ctx = to_infra(ctx);
    auto result = NlsServer::verify_proof(mutable_ctx, username,
                                          client_public_key_A,
                                          client_proof_M1);
    // Propagate any mutation of the session key etc. back to the caller.
    ctx = to_port(mutable_ctx);

    if (!result) {
        return core::fail(map_error(result.error()));
    }
    return core::Result<std::array<std::byte, 20>,
                        application::auth::NlsCryptoError>{std::move(result).value()};
}

}  // namespace pvpgn::infra::crypto
