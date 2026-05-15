// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// MySQL-backed account repository (stub).

#include <memory>

#include "application/ports/account_repository.hpp"
#include "infra/mysql/connection.hpp"

#ifdef PVPGN_V3_WITH_MYSQL

namespace pvpgn::infra::mysql {

/// MySQL implementation of IAccountRepository.
/// Currently a stub that delegates to a future real implementation.
/// All methods return NotImplemented errors with TODO comments.
class MySQLAccountRepository final : public application::ports::IAccountRepository {
public:
    explicit MySQLAccountRepository(std::shared_ptr<MySQLConnection> conn);

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
    std::shared_ptr<MySQLConnection> conn_;

    // TODO: Implement real MySQL queries using `conn_->query()` and `conn_->exec()`
    // - get_by_id: SELECT ... FROM accounts WHERE id = ?
    // - get_by_username: SELECT ... FROM accounts WHERE username = ?
    // - create: INSERT INTO accounts (...) VALUES (...)
    // - update: UPDATE accounts SET ... WHERE id = ?
    // - delete_account: DELETE FROM accounts WHERE id = ?
    // - get_all: SELECT * FROM accounts
};

}  // namespace pvpgn::infra::mysql

#endif  // PVPGN_V3_WITH_MYSQL
