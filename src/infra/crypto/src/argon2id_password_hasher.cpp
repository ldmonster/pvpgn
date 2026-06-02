// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/crypto/argon2id_password_hasher.hpp"

// Compiled only on the libsodium build matrix (CMake adds this source under
// `if(SODIUM_FOUND)` and defines PVPGN_V3_WITH_SODIUM). The guard makes an
// accidental no-sodium compile fail loudly rather than silently no-op.
#if !defined(PVPGN_V3_WITH_SODIUM)
#  error "argon2id_password_hasher.cpp requires PVPGN_V3_WITH_SODIUM (libsodium)"
#endif

#include <sodium.h>

#include <stdexcept>
#include <string>

namespace pvpgn::infra::crypto {

namespace {

/// Initialise libsodium exactly once. `sodium_init()` returns 0 on first
/// success, 1 if already initialised, and -1 on failure; only -1 is an error.
void ensure_sodium_init() {
    static const int rc = ::sodium_init();
    if (rc < 0) {
        throw std::runtime_error("Argon2idPasswordHasher: sodium_init() failed");
    }
}

}  // namespace

Argon2idPasswordHasher::Argon2idPasswordHasher(Argon2idParams params)
    : params_(params) {
    ensure_sodium_init();
}

std::string Argon2idPasswordHasher::hash(std::string_view password) {
    // crypto_pwhash_str writes a NUL-terminated PHC string (argon2id) using a
    // freshly generated random salt, sized at most crypto_pwhash_STRBYTES.
    char out[crypto_pwhash_STRBYTES];
    const int rc = ::crypto_pwhash_str(
        out,
        password.data(),
        static_cast<unsigned long long>(password.size()),
        static_cast<unsigned long long>(params_.ops_limit),
        params_.mem_limit_bytes);
    if (rc != 0) {
        // Non-zero almost always means the memory limit could not be allocated.
        throw std::runtime_error("Argon2idPasswordHasher: hashing failed "
                                 "(insufficient memory for the configured cost)");
    }
    return std::string(out);
}

bool Argon2idPasswordHasher::verify(std::string_view password,
                                    std::string_view encoded) const {
    // crypto_pwhash_str_verify wants a NUL-terminated C string; a string_view
    // is not guaranteed to be. Copy into a std::string. The comparison itself
    // is constant-time inside libsodium.
    const std::string enc(encoded);
    return ::crypto_pwhash_str_verify(
               enc.c_str(),
               password.data(),
               static_cast<unsigned long long>(password.size())) == 0;
}

bool Argon2idPasswordHasher::needs_rehash(std::string_view encoded) const {
    const std::string enc(encoded);
    // Returns 0 if the stored parameters match the current policy, 1 if a
    // rehash is warranted, and -1 if `enc` is not a recognised argon2id PHC
    // string (e.g. a legacy bnet digest). Both 1 and -1 mean "re-hash on next
    // successful login".
    const int rc = ::crypto_pwhash_str_needs_rehash(
        enc.c_str(),
        static_cast<unsigned long long>(params_.ops_limit),
        params_.mem_limit_bytes);
    return rc != 0;
}

core::crypto::PasswordHashAlgorithm
Argon2idPasswordHasher::algorithm() const noexcept {
    return core::crypto::PasswordHashAlgorithm::argon2id;
}

}  // namespace pvpgn::infra::crypto
