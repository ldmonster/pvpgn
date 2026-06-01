// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unregister_realm.hpp
/// Use case: unregister (remove) a realm server from the catalog.

#include "application/ports/ports.hpp"
#include "domain/realm/ports.hpp"
#include "core/result.hpp"
#include <cstdint>

namespace pvpgn::application::realm {

/// Input command for unregistering a realm.
struct UnregisterRealmCommand {
    std::uint32_t realm_id = 0;
};

/// Removes a realm from the repository.
///
/// Invariants enforced:
///   - The realm identified by `realm_id` must exist.
class UnregisterRealm {
public:
    explicit UnregisterRealm(application::ports::IRealmRepository& realms);

    /// Returns `NotFound` if no realm with the given ID exists.
    [[nodiscard]] core::Result<void, core::Error>
    execute(UnregisterRealmCommand cmd) const;

 private:
    application::ports::IRealmRepository& realms_;
};

} // namespace pvpgn::application::realm
