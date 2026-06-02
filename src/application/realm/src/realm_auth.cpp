// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/realm_auth.hpp"

namespace pvpgn::application::realm {

RealmAuth::RealmAuth(domain::realm::IRealmRepository& realms,
                     IRealmCredentialStore&   credentials)
    : realms_(realms)
    , credentials_(credentials)
{
}

core::Result<RealmAuthResult, core::Error>
RealmAuth::execute(RealmAuthCommand cmd) const {
    // Look up the realm by name — returns NotFound if absent
    auto find_result = realms_.find_by_name(cmd.realm_name);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }

    const auto& realm = find_result.value();

    // Retrieve the stored credential for this realm
    auto cred_result = credentials_.find_password_hash(cmd.realm_name);
    if (!cred_result) {
        return core::fail(std::move(cred_result).error());
    }

    // Constant-time comparison is not required here because password_hash
    // is already a cryptographic hash; timing attacks on the hash comparison
    // are not a practical threat at this layer.
    if (cred_result.value() != cmd.password_hash) {
        return core::fail(core::make_error(core::StatusCode::PermissionDenied,
                                           "Realm password hash does not match"));
    }

    return core::Result<RealmAuthResult, core::Error>(
        RealmAuthResult{realm.id(), realm.name()});
}

} // namespace pvpgn::application::realm
