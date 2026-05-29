// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Port: Account repository interface for hexagonal architecture.
///
/// Uses domain types (AccountId, UserName) rather than primitives so that
/// all adapters (InMemory, SQLite, File, Shadow) share the same vocabulary.

#include <cstddef>
#include <functional>

#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::ports {

/// Port: Account repository interface for hexagonal architecture.
/// Implementations provide persistence backends (InMemory, SQLite, File, etc.).
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    /// Find account by numeric ID.
    virtual core::Result<domain::identity::Account>
        find_by_id(domain::AccountId id) const = 0;

    /// Find account by name (case-insensitive lookup).
    virtual core::Result<domain::identity::Account>
        find_by_name(const domain::UserName& name) const = 0;

    /// Save or update an account.
    virtual core::Status<>
        save(const domain::identity::Account& account) = 0;

    /// Remove account by ID.
    virtual core::Status<>
        remove(domain::AccountId id) = 0;

    /// Iterate over all accounts; predicate returns false to stop early.
    virtual void
        forEach(std::function<bool(const domain::identity::Account&)> predicate)
        const = 0;

    /// Get total account count.
    virtual std::size_t size() const noexcept = 0;
};

} // namespace pvpgn::application::ports
