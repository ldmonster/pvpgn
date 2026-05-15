// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// PostgreSQL-backed account repository (stub).

#include <memory>

#include "application/ports/account_repository.hpp"
#include "infra/postgres/connection.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

namespace pvpgn::infra::postgres {

/// PostgreSQL implementation of IAccountRepository.
/// Currently a stub that delegates to a future real implementation.
/// All methods return NotImplemented errors with TODO comments.
class PostgreSQLAccountRepository final : public application::ports::IAccountRepository {
public:
    explicit PostgreSQLAccountRepository(std::shared_ptr<PostgreSQLConnection> conn);

    core::Result<domain::Account, core::Error>
    get_by_id(domain::AccountId id) override;

    core::Result<domain::Account, core::Error>
    get_by_username(std::string_view username) override;

    core::Result<domain::AccountId, core::Error>
    create(const domain::Account& account) override;

    core::Result<void, core::Error>
    update(const domain::Account& account) override;

    core::Result<void, core::Error>
    delete_account(domain::AccountId id) override;

    core::Result<std::vector<domain::Account>, core::Error>
    get_all() override;

private:
    std::shared_ptr<PostgreSQLConnection> conn_;

    // TODO: Implement real PostgreSQL queries using `conn_->query()` and `conn_->exec()`
    // Note: PostgreSQL uses $1, $2, ... for parameters instead of ?
    // - get_by_id: SELECT ... FROM accounts WHERE id = $1
    // - get_by_username: SELECT ... FROM accounts WHERE username = $1
    // - create: INSERT INTO accounts (...) VALUES ($1, $2, ...) RETURNING id
    // - update: UPDATE accounts SET ... WHERE id = $1
    // - delete_account: DELETE FROM accounts WHERE id = $1
    // - get_all: SELECT * FROM accounts
    // - Use BIGSERIAL for auto-increment IDs instead of INTEGER AUTOINCREMENT
};

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
