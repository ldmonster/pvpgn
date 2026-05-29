// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// MySQL-backed account repository (stub).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "application/ports/account_repository.hpp"
#include "infra/mysql/connection.hpp"

#ifdef PVPGN_V3_WITH_MYSQL

namespace pvpgn::infra::mysql {

/// MySQL implementation of IAccountRepository.
/// Currently a stub — all methods throw std::runtime_error until implemented.
class MySQLAccountRepository final : public application::ports::IAccountRepository {
public:
    explicit MySQLAccountRepository(std::shared_ptr<MySQLConnection> conn);

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
    std::shared_ptr<MySQLConnection> conn_;

    // TODO: Implement real MySQL queries using `conn_->query()` and `conn_->exec()`
    // - find_by_name: SELECT ... FROM accounts WHERE username = ?
    // - find_by_id:   SELECT ... FROM accounts WHERE id = ?
    // - save:         INSERT OR UPDATE accounts ...
    // - remove:       DELETE FROM accounts WHERE id = ?
    // - forEach:      SELECT ... FROM accounts (iterate all rows)
    // - size:         SELECT COUNT(*) FROM accounts
};

}  // namespace pvpgn::infra::mysql

#endif  // PVPGN_V3_WITH_MYSQL
