// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/register_realm.hpp"

#include "domain/realm/realm.hpp"

namespace pvpgn::application::realm {

RegisterRealm::RegisterRealm(ports::IRealmRepository& realms)
    : realms_(realms)
{
}

core::Result<RegisterRealmResult, core::Error>
RegisterRealm::execute(RegisterRealmCommand cmd) const {
    // Validate inputs
    if (cmd.name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Realm name cannot be empty"));
    }
    if (cmd.host.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Realm host cannot be empty"));
    }

    // Check for duplicate name
    auto existing = realms_.find_by_name(cmd.name);
    if (existing.has_value()) {
        return core::fail(core::make_error(core::StatusCode::Conflict,
                                           "A realm with that name is already registered"));
    }

    // Assign a new ID based on current repository size
    const auto new_id = static_cast<std::uint32_t>(realms_.size() + 1);

    // Create the domain aggregate
    auto create_result = domain::realm::Realm::create(new_id, cmd.name, cmd.description);
    if (!create_result) {
        return core::fail(std::move(create_result).error());
    }

    // Persist
    auto save_result = realms_.save(create_result.value());
    if (!save_result) {
        return core::fail(std::move(save_result).error());
    }

    return core::Result<RegisterRealmResult, core::Error>(
        RegisterRealmResult{new_id});
}

} // namespace pvpgn::application::realm
