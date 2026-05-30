// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file password_hasher.hpp
/// Port for deriving the legacy Battle.net session hash:
///   `hash2 = bnet_hash(client_token || server_token || hash1)`
/// where `hash1` is the persisted password digest.
///
/// The application layer uses this abstraction so that
/// `LoginUser` / `ChangePassword` never depend on legacy crypto
/// internals. Adapters live in `infra/legacy_crypto/`.

#include "domain/shared/bn_hash.hpp"

#include <cstdint>

namespace pvpgn::application::ports {

class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    /// Derive the per-connection session hash from a stored password
    /// hash and the (ticks, sessionkey) pair negotiated at logon.
    [[nodiscard]] virtual domain::BNHash
    derive_session_hash(const domain::BNHash& password_hash1,
                        std::uint32_t         ticks,
                        std::uint32_t         sessionkey) const noexcept = 0;
};

} // namespace pvpgn::application::ports
