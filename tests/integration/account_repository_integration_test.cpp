// SPDX-License-Identifier: GPL-2.0-or-later

/// @file account_repository_integration_test.cpp
/// Integration tests for AccountRepository implementations against real
/// storage backends.
///
/// SQLite and File tests always run (they use on-disk temp files).
/// MySQL and PostgreSQL tests are skipped unless the corresponding DSN
/// environment variables are set:
///   PVPGN_TEST_MYSQL_DSN   e.g. "mysql://pvpgn:pvpgn_test@localhost/pvpgn_test"
///   PVPGN_TEST_PG_DSN      e.g. "postgresql://pvpgn:pvpgn_test@localhost/pvpgn_test"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

#ifdef PVPGN_HAS_INFRA_SQLITE
#include "infra/sqlite/account_repository.hpp"
#include "infra/sqlite/connection.hpp"
#include "infra/migrations/migration_runner.hpp"
#include "infra/migrations/all_migrations.hpp"
#endif

#ifdef PVPGN_HAS_INFRA_FILE
#include "infra/file/account_repository.hpp"
#endif

namespace pvpgn::integration {

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// RAII temp directory — created on construction, removed on destruction.
class TempDir {
public:
    TempDir() {
        base_ = std::filesystem::temp_directory_path() /
                ("pvpgn_integ_test_" +
                 std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count()));
        std::filesystem::create_directories(base_);
    }

    ~TempDir() noexcept {
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
    }

    const std::filesystem::path& path() const noexcept { return base_; }
    std::string str() const { return base_.string(); }

private:
    std::filesystem::path base_;
};

/// Build a minimal test account.
domain::identity::Account make_account(std::uint32_t id,
                                       std::string_view username) {
    auto name = domain::UserName::parse(username);
    REQUIRE(name.has_value());
    return domain::identity::Account::rehydrate(
        domain::AccountId{id},
        std::move(name.value()),
        domain::BNHash{},
        domain::Locale::parse_or_default("enUS"),
        domain::identity::CommandGroupMask{},
        std::nullopt,
        false,
        false);
}

/// Return the value of an environment variable, or empty string if unset.
std::string getenv_str(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string{v} : std::string{};
}

}  // namespace

// ---------------------------------------------------------------------------
// SQLite integration tests (always run)
// ---------------------------------------------------------------------------

#ifdef PVPGN_HAS_INFRA_SQLITE

TEST_CASE("SQLiteAccountRepository: save and find by name (on-disk)",
          "[integration][sqlite]") {
    // Use a real on-disk SQLite file in a temp directory.
    TempDir tmp;
    const auto db_path = (tmp.path() / "test.db").string();

    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(db_path);

    // Apply migrations to create the schema.
    infra::migrations::MigrationRunner runner(
        [&conn](std::string_view sql) { return conn->exec(sql); },
        [&conn]() -> std::optional<std::uint32_t> {
            std::optional<std::uint32_t> version;
            conn->query(
                "SELECT MAX(version) FROM _schema_migrations",
                [&version](const infra::sqlite::Row& row) {
                    if (!row.is_null(0)) {
                        version = static_cast<std::uint32_t>(row.get_int(0));
                    }
                    return false;
                });
            return version;
        });
    runner.ensure_migration_table();
    runner.migrate_to_latest(infra::migrations::get_all_migrations());

    infra::sqlite::SQLiteAccountRepository repo(conn);

    auto account = make_account(1, "IntegUser");
    REQUIRE(repo.save(account).has_value());

    auto found = repo.find_by_name(domain::UserName::parse("IntegUser").value());
    REQUIRE(found.has_value());
    REQUIRE(std::string{found->name().display()} == "IntegUser");
    REQUIRE(found->id().value() == 1u);
}

TEST_CASE("SQLiteAccountRepository: find non-existent returns error (on-disk)",
          "[integration][sqlite]") {
    TempDir tmp;
    const auto db_path = (tmp.path() / "test2.db").string();

    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(db_path);
    infra::migrations::MigrationRunner runner(
        [&conn](std::string_view sql) { return conn->exec(sql); },
        [&conn]() -> std::optional<std::uint32_t> {
            std::optional<std::uint32_t> version;
            conn->query(
                "SELECT MAX(version) FROM _schema_migrations",
                [&version](const infra::sqlite::Row& row) {
                    if (!row.is_null(0)) {
                        version = static_cast<std::uint32_t>(row.get_int(0));
                    }
                    return false;
                });
            return version;
        });
    runner.ensure_migration_table();
    runner.migrate_to_latest(infra::migrations::get_all_migrations());

    infra::sqlite::SQLiteAccountRepository repo(conn);

    auto result = repo.find_by_name(domain::UserName::parse("NoSuchUser").value());
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SQLiteAccountRepository: duplicate save returns error (on-disk)",
          "[integration][sqlite]") {
    TempDir tmp;
    const auto db_path = (tmp.path() / "test3.db").string();

    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(db_path);
    infra::migrations::MigrationRunner runner(
        [&conn](std::string_view sql) { return conn->exec(sql); },
        [&conn]() -> std::optional<std::uint32_t> {
            std::optional<std::uint32_t> version;
            conn->query(
                "SELECT MAX(version) FROM _schema_migrations",
                [&version](const infra::sqlite::Row& row) {
                    if (!row.is_null(0)) {
                        version = static_cast<std::uint32_t>(row.get_int(0));
                    }
                    return false;
                });
            return version;
        });
    runner.ensure_migration_table();
    runner.migrate_to_latest(infra::migrations::get_all_migrations());

    infra::sqlite::SQLiteAccountRepository repo(conn);

    auto a1 = make_account(1, "DupUser");
    REQUIRE(repo.save(a1).has_value());

    auto a2 = make_account(2, "DupUser");
    auto result = repo.save(a2);
    REQUIRE_FALSE(result.has_value());
}

#endif  // PVPGN_HAS_INFRA_SQLITE

// ---------------------------------------------------------------------------
// File-based integration tests (always run)
// ---------------------------------------------------------------------------

#ifdef PVPGN_HAS_INFRA_FILE

TEST_CASE("FileAccountRepository: save and find by name (temp dir)",
          "[integration][file]") {
    TempDir tmp;
    infra::file::FileAccountRepository repo(tmp.str());

    auto account = make_account(1, "FileUser");
    REQUIRE(repo.save(account).has_value());

    auto found = repo.find_by_name(domain::UserName::parse("FileUser").value());
    REQUIRE(found.has_value());
    REQUIRE(std::string{found->name().display()} == "FileUser");
}

TEST_CASE("FileAccountRepository: find non-existent returns error (temp dir)",
          "[integration][file]") {
    TempDir tmp;
    infra::file::FileAccountRepository repo(tmp.str());

    auto result = repo.find_by_name(domain::UserName::parse("NoSuchUser").value());
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("FileAccountRepository: update existing account (temp dir)",
          "[integration][file]") {
    TempDir tmp;
    infra::file::FileAccountRepository repo(tmp.str());

    auto account = make_account(1, "UpdateMe");
    REQUIRE(repo.save(account).has_value());

    // Re-save (update) — should succeed.
    REQUIRE(repo.save(account).has_value());

    auto found = repo.find_by_name(domain::UserName::parse("UpdateMe").value());
    REQUIRE(found.has_value());
}

#endif  // PVPGN_HAS_INFRA_FILE

// ---------------------------------------------------------------------------
// MySQL integration tests (skipped unless PVPGN_TEST_MYSQL_DSN is set)
// ---------------------------------------------------------------------------

TEST_CASE("MySQLAccountRepository: save and find by name",
          "[integration][mysql][!hide]") {
    const auto dsn = getenv_str("PVPGN_TEST_MYSQL_DSN");
    if (dsn.empty()) {
        SKIP("PVPGN_TEST_MYSQL_DSN not set — skipping MySQL integration tests");
    }

#ifdef PVPGN_HAS_INFRA_MYSQL
    // TODO(R338): construct MySQLAccountRepository from dsn and run tests.
    // Placeholder until pvpgn_infra_mysql is fully implemented.
    WARN("MySQL integration test placeholder — implement when infra_mysql is ready");
#else
    SKIP("pvpgn_infra_mysql not compiled in this build");
#endif
}

// ---------------------------------------------------------------------------
// PostgreSQL integration tests (skipped unless PVPGN_TEST_PG_DSN is set)
// ---------------------------------------------------------------------------

TEST_CASE("PostgreSQLAccountRepository: save and find by name",
          "[integration][postgresql][!hide]") {
    const auto dsn = getenv_str("PVPGN_TEST_PG_DSN");
    if (dsn.empty()) {
        SKIP("PVPGN_TEST_PG_DSN not set — skipping PostgreSQL integration tests");
    }

#ifdef PVPGN_HAS_INFRA_POSTGRESQL
    // TODO(R338): construct PostgreSQLAccountRepository from dsn and run tests.
    // Placeholder until pvpgn_infra_postgresql is fully implemented.
    WARN("PostgreSQL integration test placeholder — implement when infra_postgresql is ready");
#else
    SKIP("pvpgn_infra_postgresql not compiled in this build");
#endif
}

}  // namespace pvpgn::integration
