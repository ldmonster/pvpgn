// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Plan 07: a single, driver-parameterized account repository. Replaces the
/// per-backend `infra/{sqlite,mysql,postgres}/account_repository.cpp` copies —
/// the same SQL/logic runs over any `IDbDriver`. The backend is chosen at
/// composition time by handing this repository the matching driver, so
/// switching `[storage].backend` requires no recompilation of this code.

#include <functional>
#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IAccountRepository` implemented over the backend-agnostic `IDbDriver`.
class SqlAccountRepository final : public domain::identity::IAccountRepository {
public:
    explicit SqlAccountRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override;

    [[nodiscard]] core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override;

    core::Status<> save(const domain::identity::Account& account) override;

    core::Status<> remove(domain::AccountId id) override;

    void forEach(
        std::function<bool(const domain::identity::Account&)> predicate)
        const override;

    [[nodiscard]] std::size_t size() const noexcept override;

private:
    static domain::identity::Account account_from_row(const DbRow& row);

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
