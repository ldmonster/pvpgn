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
    // Run migrations ONCE on a dedicated bootstrap connection. This handle is
    // used only here; every UoW gets its OWN connection (see create()), so the
    // bootstrap connection is closed as soon as this constructor returns.
    //
    // Why per-UoW connections: a single sqlite3* is documented single-threaded
    // (connection.hpp) and carries per-handle transaction state. Sharing one
    // handle across UoWs running on different bnetd worker threads is undefined
    // behaviour and lets one thread's COMMIT flush another thread's writes.
    SQLiteConnection bootstrap(connection_path_);

    if (bootstrap.is_open()) {
        // Setup migrations
        migrations::MigrationRunner runner(
            [&bootstrap](std::string_view sql) {
                return bootstrap.exec(sql);
            },
            [&bootstrap]() -> std::optional<std::uint32_t> {
                std::optional<std::uint32_t> version;
                (void)bootstrap.query(
                    "SELECT MAX(version) FROM _schema_migrations",
                    [&version](const sqlite::Row& row) {
                        if (!row.is_null(0)) {
                            version = static_cast<std::uint32_t>(row.get_int(0));
                        }
                        return false;
                    });
                return version;
            });

        // Ensure migration table and run pending migrations. The runner is
        // idempotent (it skips any migration whose version <= the recorded
        // current version), so re-opening an already-migrated file is a no-op.
        (void)runner.ensure_migration_table();
        (void)runner.migrate_to_latest(migrations::get_all_migrations());
    }
}

std::unique_ptr<application::ports::IUnitOfWork>
SQLiteUnitOfWorkFactory::create() {
    // One fresh connection (and therefore one independent sqlite3 handle +
    // independent transaction state) per UoW. Connections target the same DB
    // FILE, so committed data remains visible across UoW instances; SQLite's
    // file-level locking + busy timeout serialize concurrent writers safely.
    auto conn = std::make_shared<SQLiteConnection>(connection_path_);
    return std::make_unique<SQLiteUnitOfWork>(std::move(conn));
}

}  // namespace pvpgn::infra::sqlite
