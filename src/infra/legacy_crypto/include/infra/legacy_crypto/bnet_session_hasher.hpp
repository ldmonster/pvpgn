// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnet_session_hasher.hpp
/// `IPasswordHasher` adapter that derives the legacy Battle.net
/// session hash (`hash2 = bnet_hash(ticks || sessionkey || hash1)`)
/// by delegating to the broken-SHA-1 implementation in
/// `src/common/bnethash.c`. The application layer never sees the
/// algorithm itself -- only this adapter knows the bit-twiddling.

#include "domain/identity/ports.hpp"

namespace pvpgn::infra::legacy_crypto {

class BnetSessionHasher final
    : public application::ports::IPasswordHasher {
public:
    domain::BNHash derive_session_hash(
        const domain::BNHash& password_hash1,
        std::uint32_t ticks,
        std::uint32_t sessionkey) const noexcept override;
};

}  // namespace pvpgn::infra::legacy_crypto
