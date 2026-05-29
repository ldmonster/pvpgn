// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file register_realm.hpp
/// Use case: register a new realm server in the catalog.

#include "application/ports/realm_repository.hpp"
#include "core/result.hpp"
#include <cstdint>
#include <string>

namespace pvpgn::application::realm {

/// Input command for registering a realm.
struct RegisterRealmCommand {
    std::string   name;           ///< Realm name (must be unique, 1..32 chars)
    std::string   description;
    std::string   host;           ///< IP or hostname of the realm server
    std::uint16_t port  = 0;
    std::string   password_hash;  ///< Pre-hashed password for s2s auth
};

/// Result returned on successful registration.
struct RegisterRealmResult {
    std::uint32_t realm_id = 0;
};

/// Registers a new realm in the repository.
///
/// Invariants enforced:
///   - `name` must be non-empty (1..32 chars).
///   - `host` must be non-empty.
///   - No realm with the same name may already be registered.
class RegisterRealm {
public:
    explicit RegisterRealm(ports::IRealmRepository& realms);

    /// Returns `Conflict` if a realm with the same name already exists.
    /// Returns `InvalidArgument` if `name` or `host` is empty.
    [[nodiscard]] core::Result<RegisterRealmResult, core::Error>
    execute(RegisterRealmCommand cmd) const;

private:
    ports::IRealmRepository& realms_;
};

} // namespace pvpgn::application::realm
