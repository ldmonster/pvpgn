// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "domain/identity/account.hpp"
#include "core/result.hpp"
#include <string>
#include <vector>
#include <optional>

namespace pvpgn::application::ports {

/// Port: Account repository interface for hexagonal architecture.
/// Implementations provide persistence backends (InMemory, SQLite, etc.).
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    /// Find account by name (case-insensitive lookup).
    virtual core::Result<domain::identity::Account, core::Error>
        find_by_name(std::string_view name) = 0;

    /// Find account by ID.
    virtual core::Result<domain::identity::Account, core::Error>
        find_by_id(uint32_t id) = 0;

    /// Save or update an account.
    virtual core::Result<void, core::Error>
        save(const domain::identity::Account& account) = 0;

    /// Remove account by name.
    virtual core::Result<void, core::Error>
        remove(std::string_view name) = 0;

    /// Check if account exists by name.
    virtual core::Result<bool, core::Error>
        exists(std::string_view name) = 0;

    /// List all currently online accounts.
    virtual core::Result<std::vector<domain::identity::Account>, core::Error>
        list_online() = 0;

    /// Get total account count.
    virtual core::Result<uint32_t, core::Error>
        count() = 0;
};

} // namespace pvpgn::application::ports
