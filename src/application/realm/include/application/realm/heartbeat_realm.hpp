// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file heartbeat_realm.hpp
/// Use case: record a keep-alive heartbeat from a connected realm server.
///
/// The domain `Realm` aggregate does not carry a `last_seen` timestamp;
/// the heartbeat therefore confirms the realm is still reachable and
/// re-saves the aggregate so that any infrastructure-layer adapter can
/// update its own timestamp column / field.

#include "application/ports/ports.hpp"
#include "domain/realm/ports.hpp"
#include "core/clock.hpp"
#include "core/result.hpp"
#include <cstdint>

namespace pvpgn::application::realm {

/// Input command for a realm heartbeat.
struct HeartbeatRealmCommand {
    std::uint32_t    realm_id = 0;
    core::SystemTime now;
};

/// Processes a keep-alive heartbeat from a realm server.
///
/// Invariants enforced:
///   - The realm identified by `realm_id` must exist.
class HeartbeatRealm {
public:
    explicit HeartbeatRealm(application::ports::IRealmRepository& realms);

    /// Returns `NotFound` if no realm with the given ID exists.
    /// On success the realm is re-saved so infrastructure adapters can
    /// update their own `last_seen` timestamp.
    [[nodiscard]] core::Result<void, core::Error>
    execute(HeartbeatRealmCommand cmd) const;

private:
    application::ports::IRealmRepository& realms_;
};

} // namespace pvpgn::application::realm
