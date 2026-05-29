// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_registry.hpp
/// Bi-directional `SessionId ↔ AccountId` map. Owned by the
/// composition root and shared across protocol adapters so a single
/// user maps to a single connection at any moment.

#include <optional>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

class ISessionRegistry {
public:
    virtual ~ISessionRegistry() = default;

    /// Bind a session to an account. Returns `AlreadyExists` if the
    /// account already has an open session (caller decides on a
    /// re-login policy — see `LoginUser`).
    virtual core::Status<>
    attach(domain::SessionId session, domain::AccountId account) = 0;

    /// Drop a binding by session. No-op if absent.
    virtual void detach(domain::SessionId session) = 0;

    virtual std::optional<domain::SessionId>
    session_for(domain::AccountId account) const = 0;

    virtual std::optional<domain::AccountId>
    account_for(domain::SessionId session) const = 0;

    virtual std::vector<domain::SessionId> list() const = 0;
};

}  // namespace pvpgn::application::ports
