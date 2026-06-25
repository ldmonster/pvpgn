// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnet_session_hasher.hpp
/// Production IPasswordHasher for the OLS (SID_LOGONRESPONSE2) login flow.

#include "domain/identity/ports.hpp"

namespace pvpgn::infra::crypto {

/// Derives the per-connection session hash ("hash2") the Battle.net OLS login
/// expects: the broken-SHA-1 of `ticks ‖ sessionkey ‖ stored_hash1`, matching
/// the original server's `temp{ ticks; sessionkey; passhash1[5] }` digest
/// (handle_bnet.cpp). Stateless and thread-safe.
class BnetSessionHasher final : public domain::identity::IPasswordHasher {
public:
    [[nodiscard]] domain::BNHash
    derive_session_hash(const domain::BNHash& password_hash1,
                        std::uint32_t         ticks,
                        std::uint32_t         sessionkey) const noexcept override;
};

}  // namespace pvpgn::infra::crypto
