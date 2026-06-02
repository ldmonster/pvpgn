// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file sqlite_driver.hpp
/// SQLite driver implementation of IDbDriver interface.

#include <memory>
#include <string_view>

#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::sqlite {
class SQLiteConnection;
}

namespace pvpgn::infra::persistence {

/// SQLite implementation of the database driver interface.
class SqliteDriver final : public IDbDriver {
public:
    explicit SqliteDriver(std::shared_ptr<pvpgn::infra::sqlite::SQLiteConnection> conn);
    ~SqliteDriver() override;

    core::Result<void, core::Error> exec(std::string_view sql) override;

    core::Result<void, core::Error> query(
        std::string_view sql,
        const DbRowCallback& cb) override;

    core::Result<void, core::Error> query_bind(
        std::string_view sql,
        std::initializer_list<DbParamValue> params,
        const DbRowCallback& cb) override;

    std::int64_t last_insert_rowid() const override;

    core::Result<void, core::Error> begin_transaction() override;
    core::Result<void, core::Error> commit() override;
    core::Result<void, core::Error> rollback() override;
    bool in_transaction() const override;

private:
    std::shared_ptr<pvpgn::infra::sqlite::SQLiteConnection> conn_;
    // Transactions nest via SAVEPOINTs: the outermost begin/commit/rollback
    // maps to BEGIN/COMMIT/ROLLBACK, inner ones to SAVEPOINT/RELEASE/
    // ROLLBACK TO. This lets a Unit-of-Work transaction wrap a repository's own
    // multi-statement transaction without SQLite's "cannot start a transaction
    // within a transaction" error.
    int tx_depth_;  ///< 0 = no transaction; >0 = nesting depth
};

}  // namespace pvpgn::infra::persistence
