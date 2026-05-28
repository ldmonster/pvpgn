// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// PostgreSQL-backed account repository (stub).

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "application/ports/account_repository.hpp"
#include "infra/postgres/connection.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

namespace pvpgn::infra::postgres {

/// PostgreSQL implementation of IAccountRepository.
/// Currently a stub — all methods throw std::runtime_error until implemented.
class PostgreSQLAccountRepository final : public application::ports::IAccountRepository {
public:
    explicit PostgreSQLAccountRepository(std::shared_ptr<PostgreSQLConnection> conn);

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
    std::shared_ptr<PostgreSQLConnection> conn_;

    // TODO: Implement real PostgreSQL queries using `conn_->query()` and `conn_->exec()`
    // Note: PostgreSQL uses $1, $2, ... for parameters instead of ?
    // - find_by_name: SELECT ... FROM accounts WHERE username = $1
    // - find_by_id:   SELECT ... FROM accounts WHERE id = $1
    // - save:         INSERT INTO accounts (...) VALUES ($1, ...) ON CONFLICT DO UPDATE
    // - remove:       DELETE FROM accounts WHERE username = $1
    // - exists:       SELECT COUNT(*) FROM accounts WHERE username = $1
    // - list_online:  SELECT ... FROM accounts WHERE online = TRUE
    // - count:        SELECT COUNT(*) FROM accounts
    // - Use BIGSERIAL for auto-increment IDs instead of INTEGER AUTOINCREMENT
};

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
