// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// SQLite-backed account persistence.

#include <memory>

#include "domain/identity/ports.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteAccountRepository final
    : public application::ports::IAccountRepository {
public:
    explicit SQLiteAccountRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override;

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override;

    core::Status<> save(const domain::identity::Account& account) override;

    core::Status<> remove(domain::AccountId id) override;

    void forEach(std::function<bool(const domain::identity::Account&)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;

    /// Load an account from a row snapshot (used by query callbacks)
    static domain::identity::Account account_from_row(const Row& row);
};

}  // namespace pvpgn::infra::sqlite
