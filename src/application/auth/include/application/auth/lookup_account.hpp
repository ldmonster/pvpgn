// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file lookup_account.hpp
/// Read-only query use-case: look up a single account by name and
/// report whether it currently has an active session.
///
/// Collaborators are constructor-injected by reference. The use-case
/// is pure: no globals, no threads, no I/O of its own.

#include <string>
#include <string_view>

#include "application/ports/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/identity/ports.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::auth {

struct LookupAccountResult {
    domain::AccountId id;
    std::string       name;
    bool              is_locked;
    bool              is_online;
};

class LookupAccountByName {
public:
    explicit LookupAccountByName(
        application::ports::IAccountRepository& repo,
        application::ports::ISessionRegistry&   sessions) noexcept
        : repo_(repo), sessions_(sessions) {}

    /// Returns the account info, or a `NotFound` error if no such account
    /// exists in the repository.
    [[nodiscard]] core::Status<LookupAccountResult>
    execute(std::string_view name) const;

private:
    application::ports::IAccountRepository& repo_;
    application::ports::ISessionRegistry&   sessions_;
};

}  // namespace pvpgn::application::auth
