// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/unit_of_work_factory.hpp"

#include "infra/sqlite/unit_of_work.hpp"
#include "infra/migrations/migration_runner.hpp"
#include "infra/migrations/all_migrations.hpp"

#include "application/persistence/unit_of_work.hpp"

namespace pvpgn::infra::sqlite {

SQLiteUnitOfWorkFactory::SQLiteUnitOfWorkFactory(
    std::string_view connection_path)
    : connection_path_(connection_path) {
    // Open shared connection and run migrations
    conn_ = std::make_shared<SQLiteConnection>(connection_path_);

    if (conn_->is_open()) {
        // Setup migrations
        migrations::MigrationRunner runner(
            [this](std::string_view sql) {
                return conn_->exec(sql);
            },
            [this]() -> std::optional<std::uint32_t> {
                std::optional<std::uint32_t> version;
                (void)conn_->query(
                    "SELECT MAX(version) FROM _schema_migrations",
                    [&version](const sqlite::Row& row) {
                        if (!row.is_null(0)) {
                            version = static_cast<std::uint32_t>(row.get_int(0));
                        }
                        return false;
                    });
                return version;
            });

        // Ensure migration table and run pending migrations
        (void)runner.ensure_migration_table();
        (void)runner.migrate_to_latest(migrations::get_all_migrations());
    }
}

std::unique_ptr<application::ports::IUnitOfWork>
SQLiteUnitOfWorkFactory::create() {
    return std::make_unique<SQLiteUnitOfWork>(conn_);
}

}  // namespace pvpgn::infra::sqlite
