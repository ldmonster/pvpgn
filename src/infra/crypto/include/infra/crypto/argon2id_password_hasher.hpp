// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file argon2id_password_hasher.hpp
/// `Argon2idPasswordHasher` — the at-rest password hashing adapter.
///
/// Implements `core::crypto::IPasswordHasher` over libsodium's
/// `crypto_pwhash_str*` family (the ARGON2ID13 algorithm). Output is the
/// self-describing PHC string `$argon2id$v=19$m=...,t=...,p=...$salt$digest`,
/// so `verify` / `needs_rehash` need no out-of-band parameters.
///
/// **Build gating:** the implementation (`argon2id_password_hasher.cpp`) is
/// compiled only when the build is configured with libsodium
/// (`PVPGN_V3_WITH_SODIUM`). This header is dependency-free and
/// always declarable; constructing the class in a build without libsodium is a
/// link error by design — composition roots wire it only on the libsodium
/// matrix.
///
/// This is the password-AT-REST hasher; it is unrelated to the legacy
/// session-hash port `domain::identity::IPasswordHasher`.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/crypto/password_hasher.hpp"

namespace pvpgn::infra::crypto {

/// Argon2id cost parameters. Defaults: `t=2, m=64 MiB`;
/// libsodium's `crypto_pwhash_str` fixes parallelism at `p=1`. Exposed so a
/// composition root can load them from `[auth.argon2id]` in `bnetd.toml`.
struct Argon2idParams {
    /// Operations limit (argon2 time cost, `t`). libsodium default "moderate"
    /// is `crypto_pwhash_OPSLIMIT_MODERATE` (3); we pick 2.
    std::uint64_t ops_limit = 2;

    /// Memory limit in bytes (argon2 memory cost, `m`). 64 MiB.
    std::size_t mem_limit_bytes = static_cast<std::size_t>(64) * 1024 * 1024;
};

/// argon2id password-at-rest hasher backed by libsodium.
class Argon2idPasswordHasher final : public core::crypto::IPasswordHasher {
public:
    /// @throws std::runtime_error if libsodium fails to initialise.
    explicit Argon2idPasswordHasher(Argon2idParams params = {});

    /// @throws std::runtime_error on hashing failure (e.g. out of memory).
    [[nodiscard]] std::string hash(std::string_view password) override;

    [[nodiscard]] bool verify(std::string_view password,
                              std::string_view encoded) const override;

    [[nodiscard]] bool needs_rehash(std::string_view encoded) const override;

    [[nodiscard]] core::crypto::PasswordHashAlgorithm
    algorithm() const noexcept override;

    [[nodiscard]] const Argon2idParams& params() const noexcept { return params_; }

private:
    Argon2idParams params_;
};

}  // namespace pvpgn::infra::crypto
