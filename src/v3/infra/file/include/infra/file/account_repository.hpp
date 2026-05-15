// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Legacy file-based account persistence (reads .plain files).

#include <memory>
#include <shared_mutex>
#include <string>

#include "application/ports/account_repository.hpp"
#include "infra/inmemory/account_repository.hpp"

namespace pvpgn::infra::file {

class FileAccountRepository final : public application::ports::IAccountRepository {
public:
    /// Create a file repository that loads from var/users/ directory.
    /// @param data_dir Directory containing account files (e.g., "var/users/")
    explicit FileAccountRepository(std::string_view data_dir);

    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override;

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override;

    core::Status<> save(const domain::identity::Account& account) override;

    core::Status<> remove(domain::AccountId id) override;

    void forEach(std::function<bool(const domain::identity::Account&)> predicate)
        const override;

    std::size_t size() const noexcept override;

    /// Load all accounts from files in data directory.
    /// Called automatically on construction.
    void load_all();

private:
    std::string data_dir_;
    mutable std::shared_mutex cache_mutex_;
    // Use in-memory repository as backing store
    std::unique_ptr<inmemory::InMemoryAccountRepository> cache_;

    /// Load a single account from .plain file.
    /// Returns nullopt if file is invalid or unreadable.
    std::optional<domain::identity::Account> load_account_file(
        std::string_view filename);
};

}  // namespace pvpgn::infra::file
