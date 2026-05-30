// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_registry.hpp
/// Application-layer port for the bidirectional session ↔ account map.

#include <optional>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

class ISessionRegistry {
public:
    virtual ~ISessionRegistry() = default;

    ISessionRegistry(const ISessionRegistry&)            = delete;
    ISessionRegistry& operator=(const ISessionRegistry&) = delete;
    ISessionRegistry(ISessionRegistry&&)                 = delete;
    ISessionRegistry& operator=(ISessionRegistry&&)      = delete;

    virtual core::Status<> attach(domain::SessionId session,
                                  domain::AccountId account) = 0;

    virtual void detach(domain::SessionId session) = 0;

    [[nodiscard]] virtual std::optional<domain::SessionId>
    session_for(domain::AccountId account) const = 0;

    [[nodiscard]] virtual std::optional<domain::AccountId>
    account_for(domain::SessionId session) const = 0;

    [[nodiscard]] virtual std::vector<domain::SessionId> list() const = 0;

protected:
    ISessionRegistry() = default;
};

} // namespace pvpgn::application::ports
