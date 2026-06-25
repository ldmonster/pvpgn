// SPDX-License-Identifier: GPL-2.0-or-later

/// @file migration_runner_integration_test.cpp
/// Integration tests for the schema MigrationRunner against a real on-disk
/// SQLite database. Every deployment depends on this component bringing a blank
/// or partially-migrated database up to the current schema.
///
/// The unit tests (tests/unit/infra/migrations) drive the runner with a *fake*
/// SQL executor that merely records statements — they never execute DDL. These
/// tests run the real embedded migrations (`get_all_migrations()`) through a
/// real `SQLiteConnection` and assert the observable effects: the actual tables
/// appear, the applied version is persisted in `_schema_migrations` and read
/// back, re-running is idempotent, and a partial migration can be completed.
///
/// SQLite tests always run (on-disk temp files; no external services).

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#ifdef PVPGN_HAS_INFRA_SQLITE
#include "infra/migrations/all_migrations.hpp"
#include "infra/migrations/migration_runner.hpp"
#include "infra/sqlite/connection.hpp"
#endif

namespace pvpgn::integration {

#ifdef PVPGN_HAS_INFRA_SQLITE

namespace {

/// RAII temp directory — created on construction, removed on destruction.
class TempDir {
public:
    TempDir() {
        base_ = std::filesystem::temp_directory_path() /
                ("pvpgn_migrate_integ_" +
                 std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count()));
        std::filesystem::create_directories(base_);
    }

    ~TempDir() noexcept {
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
    }

    std::string db_file() const { return (base_ / "schema.db").string(); }

private:
    std::filesystem::path base_;
};

/// Build a MigrationRunner wired to a real SQLite connection. The version query
/// reads MAX(version) from the real `_schema_migrations` table (the same wiring
/// the production factory uses).
infra::migrations::MigrationRunner make_runner(
    const std::shared_ptr<infra::sqlite::SQLiteConnection>& conn) {
    return infra::migrations::MigrationRunner(
        [conn](std::string_view sql) { return conn->exec(sql); },
        [conn]() -> std::optional<std::uint32_t> {
            std::optional<std::uint32_t> version;
            (void)conn->query(
                "SELECT MAX(version) FROM _schema_migrations",
                [&version](const infra::sqlite::Row& row) {
                    if (!row.is_null(0)) {
                        version = static_cast<std::uint32_t>(row.get_int(0));
                    }
                    return false;
                });
            return version;
        });
}

/// True if a table with the given name exists in the SQLite catalogue.
bool table_exists(const std::shared_ptr<infra::sqlite::SQLiteConnection>& conn,
                  std::string_view name) {
    bool found = false;
    (void)conn->query_bind(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name = ?",
        {std::string{name}},
        [&found](const infra::sqlite::Row&) {
            found = true;
            return false;
        });
    return found;
}

/// Count rows in the `_schema_migrations` tracking table.
std::int64_t recorded_migration_count(
    const std::shared_ptr<infra::sqlite::SQLiteConnection>& conn) {
    std::int64_t n = 0;
    (void)conn->query("SELECT COUNT(*) FROM _schema_migrations",
                      [&n](const infra::sqlite::Row& row) {
                          n = row.get_int(0);
                          return false;
                      });
    return n;
}

// Tables the embedded migrations must create (001 initial schema + 002 channels).
constexpr std::string_view kExpectedTables[] = {
    "accounts", "account_attributes", "account_bans", "ip_bans",
    "ip_ban_ranges", "clans", "clan_members", "friends", "ladder", "realms",
    "channels",
};

}  // namespace

TEST_CASE("MigrationRunner over SQLite: migrate_to_latest builds the full schema",
          "[integration][sqlite][migrations]") {
    TempDir tmp;
    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(tmp.db_file());
    auto runner = make_runner(conn);

    REQUIRE(runner.ensure_migration_table().has_value());

    const auto all = infra::migrations::get_all_migrations();
    auto applied = runner.migrate_to_latest(all);
    REQUIRE(applied.has_value());
    REQUIRE(applied.value() == all.back().version);

    for (auto t : kExpectedTables) {
        INFO("expected table: " << t);
        REQUIRE(table_exists(conn, t));
    }

    // The applied version is persisted and reflected by current_version().
    REQUIRE(runner.current_version() == all.back().version);
    REQUIRE(recorded_migration_count(conn) ==
            static_cast<std::int64_t>(all.size()));
}

TEST_CASE("MigrationRunner over SQLite: re-running migrate_to_latest is idempotent",
          "[integration][sqlite][migrations]") {
    TempDir tmp;
    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(tmp.db_file());
    auto runner = make_runner(conn);

    const auto all = infra::migrations::get_all_migrations();
    REQUIRE(runner.ensure_migration_table().has_value());
    REQUIRE(runner.migrate_to_latest(all).has_value());

    const auto count_after_first = recorded_migration_count(conn);

    // Second pass: nothing pending. Must succeed, report the same version, and
    // not re-record (or duplicate) any migration rows.
    auto again = runner.migrate_to_latest(all);
    REQUIRE(again.has_value());
    REQUIRE(again.value() == all.back().version);
    REQUIRE(recorded_migration_count(conn) == count_after_first);
}

TEST_CASE("MigrationRunner over SQLite: migrate_to applies a partial schema, "
          "then completes it",
          "[integration][sqlite][migrations]") {
    TempDir tmp;
    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(tmp.db_file());
    auto runner = make_runner(conn);

    const auto all = infra::migrations::get_all_migrations();
    REQUIRE(runner.ensure_migration_table().has_value());

    // Migrate only to version 1: the initial-schema tables exist, but the
    // channels table (introduced by migration 2) must NOT yet.
    auto partial = runner.migrate_to(all, 1);
    REQUIRE(partial.has_value());
    REQUIRE(partial.value() == 1u);
    REQUIRE(table_exists(conn, "accounts"));
    REQUIRE_FALSE(table_exists(conn, "channels"));
    REQUIRE(runner.current_version() == 1u);

    // Completing the migration brings in the remaining schema.
    auto complete = runner.migrate_to_latest(all);
    REQUIRE(complete.has_value());
    REQUIRE(complete.value() == all.back().version);
    REQUIRE(table_exists(conn, "channels"));
    REQUIRE(runner.current_version() == all.back().version);
}

TEST_CASE("MigrationRunner over SQLite: applied version survives a fresh runner "
          "on the same database",
          "[integration][sqlite][migrations]") {
    TempDir tmp;
    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(tmp.db_file());

    const auto all = infra::migrations::get_all_migrations();
    {
        auto runner = make_runner(conn);
        REQUIRE(runner.ensure_migration_table().has_value());
        REQUIRE(runner.migrate_to_latest(all).has_value());
    }

    // A brand-new runner over the same connection reads the persisted version
    // out of the real _schema_migrations table — proving version tracking is
    // durable, not in-memory state of the runner.
    auto fresh = make_runner(conn);
    REQUIRE(fresh.current_version() == all.back().version);

    // And it treats the DB as already up to date (idempotent no-op).
    auto noop = fresh.migrate_to_latest(all);
    REQUIRE(noop.has_value());
    REQUIRE(noop.value() == all.back().version);
}

#endif  // PVPGN_HAS_INFRA_SQLITE

}  // namespace pvpgn::integration
