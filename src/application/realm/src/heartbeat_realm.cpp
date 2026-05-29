// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/heartbeat_realm.hpp"

namespace pvpgn::application::realm {

HeartbeatRealm::HeartbeatRealm(ports::IRealmRepository& realms)
    : realms_(realms)
{
}

core::Result<void, core::Error>
HeartbeatRealm::execute(HeartbeatRealmCommand cmd) const {
    // Verify the realm exists
    auto find_result = realms_.find_by_id(cmd.realm_id);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }

    // Re-save the aggregate so that infrastructure adapters can update
    // their own last_seen / heartbeat timestamp.
    return realms_.save(find_result.value());
}

} // namespace pvpgn::application::realm
