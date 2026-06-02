// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_auth.hpp
/// Use case: authenticate a connecting realm server by name + password hash.
///
/// The `domain::realm::Realm` aggregate does not store credentials; the
/// password hash is kept in a separate `IRealmCredentialStore` port so
/// that the domain model stays clean and credential storage can be
/// swapped independently (e.g. in-memory for tests, DB for production).

#include "domain/realm/ports.hpp"
#include "core/result.hpp"
#include <cstdint>
#include <string>

namespace pvpgn::application::realm {

/// Port: look up the stored password hash for a realm by name.
///
/// Implementations live in `infra/`; an inline fake is used in tests.
class IRealmCredentialStore {
public:
    virtual ~IRealmCredentialStore() = default;

    /// Returns the stored password hash for the named realm, or
    /// `NotFound` if no entry exists.
    virtual core::Result<std::string, core::Error>
    find_password_hash(const std::string& realm_name) const = 0;
};

/// Input command for realm authentication.
struct RealmAuthCommand {
    std::string realm_name;
    std::string password_hash;  ///< Hash provided by the connecting realm server
};

/// Result returned on successful authentication.
struct RealmAuthResult {
    std::uint32_t realm_id   = 0;
    std::string   realm_name;
};

/// Authenticates a realm server by name and password hash.
///
/// Invariants enforced:
///   - The realm name must be registered in the repository.
///   - The provided `password_hash` must match the stored credential.
class RealmAuth {
public:
    RealmAuth(domain::realm::IRealmRepository& realms,
              IRealmCredentialStore&   credentials);

    /// Returns `NotFound` if the realm name is not registered.
    /// Returns `PermissionDenied` if the password hash does not match.
    [[nodiscard]] core::Result<RealmAuthResult, core::Error>
    execute(RealmAuthCommand cmd) const;

 private:
    domain::realm::IRealmRepository& realms_;
    IRealmCredentialStore&   credentials_;
};

} // namespace pvpgn::application::realm
