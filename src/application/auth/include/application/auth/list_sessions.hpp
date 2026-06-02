// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file list_sessions.hpp
/// Read-only query use-case: enumerate all currently active sessions
/// and enrich each entry with the account name from the repository.
///
/// Collaborators are constructor-injected by reference. The use-case
/// is pure: no globals, no threads, no I/O of its own.

#include <string>
#include <vector>

#include "domain/identity/ports.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::auth {

struct SessionInfo {
    domain::AccountId   account_id;
    std::string         account_name;
};

class ListSessions {
public:
    explicit ListSessions(
        domain::identity::ISessionRegistry&   sessions,
        domain::identity::IAccountRepository& repo) noexcept
        : sessions_(sessions), repo_(repo) {}

    /// Returns all currently active sessions, each enriched with the
    /// account name. Accounts that cannot be resolved are skipped.
    [[nodiscard]] core::Status<std::vector<SessionInfo>> execute() const;

private:
    domain::identity::ISessionRegistry&   sessions_;
    domain::identity::IAccountRepository& repo_;
};

}  // namespace pvpgn::application::auth
