// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// MySQL-backed account repository (stub).

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "application/ports/account_repository.hpp"
#include "infra/mysql/connection.hpp"

#ifdef PVPGN_V3_WITH_MYSQL

namespace pvpgn::infra::mysql {

/// MySQL implementation of IAccountRepository.
/// Currently a stub — all methods throw std::runtime_error until implemented.
class MySQLAccountRepository final : public application::ports::IAccountRepository {
public:
    explicit MySQLAccountRepository(std::shared_ptr<MySQLConnection> conn);

    core::Result<domain::identity::Account, core::Error>
    find_by_name(std::string_view name) override;

    core::Result<domain::identity::Account, core::Error>
    find_by_id(uint32_t id) override;

    core::Result<void, core::Error>
    save(const domain::identity::Account& account) override;

    core::Result<void, core::Error>
    remove(std::string_view name) override;

    core::Result<bool, core::Error>
    exists(std::string_view name) override;

    core::Result<std::vector<domain::identity::Account>, core::Error>
    list_online() override;

    core::Result<uint32_t, core::Error>
    count() override;

private:
    std::shared_ptr<MySQLConnection> conn_;

    // TODO: Implement real MySQL queries using `conn_->query()` and `conn_->exec()`
    // - find_by_name: SELECT ... FROM accounts WHERE username = ?
    // - find_by_id:   SELECT ... FROM accounts WHERE id = ?
    // - save:         INSERT OR UPDATE accounts ...
    // - remove:       DELETE FROM accounts WHERE username = ?
    // - exists:       SELECT COUNT(*) FROM accounts WHERE username = ?
    // - list_online:  SELECT ... FROM accounts WHERE online = 1
    // - count:        SELECT COUNT(*) FROM accounts
};

}  // namespace pvpgn::infra::mysql

#endif  // PVPGN_V3_WITH_MYSQL
