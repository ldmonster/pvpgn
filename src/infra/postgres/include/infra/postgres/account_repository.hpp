// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// PostgreSQL-backed account repository (stub).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "domain/identity/ports.hpp"
#include "infra/postgres/connection.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

namespace pvpgn::infra::postgres {

/// PostgreSQL implementation of IAccountRepository.
/// Currently a stub — all methods throw std::runtime_error until implemented.
class PostgreSQLAccountRepository final : public application::ports::IAccountRepository {
public:
    explicit PostgreSQLAccountRepository(std::shared_ptr<PostgreSQLConnection> conn);

    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override;

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override;

    core::Status<>
    save(const domain::identity::Account& account) override;

    core::Status<>
    remove(domain::AccountId id) override;

    void forEach(
        std::function<bool(const domain::identity::Account&)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<PostgreSQLConnection> conn_;

    // TODO: Implement real PostgreSQL queries using `conn_->query()` and `conn_->exec()`
    // Note: PostgreSQL uses $1, $2, ... for parameters instead of ?
    // - find_by_name: SELECT ... FROM accounts WHERE username = $1
    // - find_by_id:   SELECT ... FROM accounts WHERE id = $1
    // - save:         INSERT INTO accounts (...) VALUES ($1, ...) ON CONFLICT DO UPDATE
    // - remove:       DELETE FROM accounts WHERE id = $1
    // - forEach:      SELECT ... FROM accounts (iterate all rows)
    // - size:         SELECT COUNT(*) FROM accounts
    // - Use BIGSERIAL for auto-increment IDs instead of INTEGER AUTOINCREMENT
};

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
