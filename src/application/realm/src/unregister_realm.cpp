// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/unregister_realm.hpp"

namespace pvpgn::application::realm {

UnregisterRealm::UnregisterRealm(domain::realm::IRealmRepository& realms)
    : realms_(realms)
{
}

core::Result<void, core::Error>
UnregisterRealm::execute(UnregisterRealmCommand cmd) const {
    // Verify the realm exists before attempting removal
    auto find_result = realms_.find_by_id(cmd.realm_id);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }

    return realms_.remove(cmd.realm_id);
}

} // namespace pvpgn::application::realm
