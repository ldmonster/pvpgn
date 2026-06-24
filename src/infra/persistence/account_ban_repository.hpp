// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_ban_repository.hpp
/// A single, driver-parameterized account-ban repository. The same SQL/logic
/// runs over any `IDbDriver`, so the storage backend is chosen at composition
/// time and switching `[storage].backend` needs no recompilation.

#include <functional>
#include <memory>
#include <optional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IAccountBanRepository` implemented over the backend-agnostic `IDbDriver`.
///
/// Schema (table `account_bans`):
///   account_id INTEGER PRIMARY KEY,  -- the banned account
///   banned_by  INTEGER,              -- the issuing account
///   reason     TEXT,
///   banned_at  INTEGER,              -- unix epoch seconds
///   expires_at INTEGER NULL          -- epoch seconds; NULL = permanent
class SqlAccountBanRepository final
    : public domain::moderation::IAccountBanRepository {
public:
    explicit SqlAccountBanRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<std::optional<domain::moderation::AccountBan>>
    find_active_ban(domain::AccountId account_id,
                    core::SystemTime now) const override;

    core::Status<> add_ban(const domain::moderation::AccountBan& ban) override;

    core::Status<> remove_ban(domain::AccountId account_id) override;

    void for_each(
        std::function<bool(const domain::moderation::AccountBan&)> predicate)
        const override;

private:
    static domain::moderation::AccountBan ban_from_row(const DbRow& row);

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
