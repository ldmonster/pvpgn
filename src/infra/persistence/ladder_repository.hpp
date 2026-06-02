// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder_repository.hpp
/// Plan 07: a single, driver-parameterized ladder repository over `IDbDriver`.
/// Replaces the per-backend `infra/{sqlite,mysql,postgres}/ladder_repository.cpp`
/// (the SQLite one was an `Unimplemented` stub).

#include <cstdint>
#include <memory>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/ladder/ladder.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `ILadderRepository` over the backend-agnostic `IDbDriver`.
///
/// Schema (table `ladder`):
///   account_id  INTEGER PRIMARY KEY,
///   rating      INTEGER,
///   wins        INTEGER,
///   losses      INTEGER,
///   disconnects INTEGER
///
/// Rank is "1 + the number of accounts with a strictly higher rating", so
/// equal-rating accounts share a rank.
class SqlLadderRepository final : public domain::ladder::ILadderRepository {
public:
    explicit SqlLadderRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<std::uint32_t, core::Error>
    get_rank(domain::AccountId account_id) override;

    core::Result<void, core::Error>
    save_entry(const domain::ladder::LadderEntry& entry) override;

    [[nodiscard]] core::Result<std::vector<domain::ladder::LadderEntry>,
                               core::Error>
    get_top_n(std::uint32_t n) override;

private:
    static domain::ladder::LadderEntry entry_from_row(const DbRow& row);

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
