// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file password_hasher.hpp
/// Port for the legacy Battle.net "session-hash" transcript.
///
/// Battle.net's CLIENT_LOGINREQ1 / CLIENT_CHANGEPASSREQ packets
/// never carry the cleartext password hash. Instead the client
/// computes:
///
///   hash2 = bnet_hash(ticks || sessionkey || hash1)
///
/// where `bnet_hash` is the broken-SHA-1 implementation in
/// `src/common/bnethash.c`. The server, knowing `hash1` (from
/// `t_account::passhash1`), re-derives `hash2` the same way and
/// compares.
///
/// `IPasswordHasher` models the `hash2`-derivation step as a port
/// so the application layer never depends on a concrete hash
/// algorithm. The production adapter
/// (`infra/legacy_crypto::BnetSessionHasher`) delegates to
/// `pvpgn::bnet_hash`; tests can install a deterministic fake.

#include <cstdint>

#include "domain/shared/bn_hash.hpp"

namespace pvpgn::application::ports {

class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    /// Derive the session-hash that the legacy client would send
    /// for the given (hash1, ticks, sessionkey) triple. The output
    /// is itself a 20-byte BNHash so the use-case can compare via
    /// constant-time `BNHash::equals_constant_time`.
    virtual domain::BNHash derive_session_hash(
        const domain::BNHash& password_hash1,
        std::uint32_t ticks,
        std::uint32_t sessionkey) const noexcept = 0;
};

}  // namespace pvpgn::application::ports
