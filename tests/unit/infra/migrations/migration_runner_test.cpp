// SPDX-License-Identifier: GPL-2.0-or-later

/// @file migration_runner_test.cpp
/// Catch2 unit tests for MigrationRunner using in-memory SQL executor stubs.
///
/// The MigrationRunner is tested in isolation: we inject a fake SqlExecutor
/// and a fake VersionQuery so no real database is required.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "infra/migrations/migration_runner.hpp"

namespace pvpgn::infra::migrations {

namespace {

/// Fake SQL executor that records every SQL string it receives.
/// Returns success unless `fail_on` matches the SQL.
struct FakeExecutor {
    std::vector<std::string> executed;
    std::string              fail_on;   ///< If non-empty, fail when SQL contains this substring.

    core::Result<void, core::Error> operator()(std::string_view sql) {
        executed.emplace_back(sql);
        if (!fail_on.empty() && std::string{sql}.find(fail_on) != std::string::npos) {
            return core::fail(core::Error{core::StatusCode::Internal, "injected failure"});
        }
        return core::ok();
    }
};

/// Fake version query that returns a fixed value.
struct FakeVersionQuery {
    std::optional<std::uint32_t> version;

    std::optional<std::uint32_t> operator()() const {
        return version;
    }
};

/// Build a simple set of test migrations.
constexpr std::string_view kMig1Up = "CREATE TABLE accounts (id INTEGER PRIMARY KEY);";
constexpr std::string_view kMig2Up = "ALTER TABLE accounts ADD COLUMN name TEXT;";

const Migration kMig1{1, "001_initial", kMig1Up, ""};
const Migration kMig2{2, "002_add_name", kMig2Up, ""};

}  // namespace

// ---------------------------------------------------------------------------
// ensure_migration_table
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: ensure_migration_table executes CREATE TABLE",
          "[infra][migrations]") {
    FakeExecutor exec;
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    auto result = runner.ensure_migration_table();
    REQUIRE(result.has_value());
    REQUIRE(exec.executed.size() == 1u);
    // The SQL must mention _schema_migrations
    REQUIRE(exec.executed[0].find("_schema_migrations") != std::string::npos);
}

TEST_CASE("MigrationRunner: ensure_migration_table propagates executor error",
          "[infra][migrations]") {
    FakeExecutor exec;
    exec.fail_on = "_schema_migrations";
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    auto result = runner.ensure_migration_table();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::Internal);
}

// ---------------------------------------------------------------------------
// current_version
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: current_version returns nullopt when no migrations applied",
          "[infra][migrations]") {
    FakeExecutor exec;
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    REQUIRE_FALSE(runner.current_version().has_value());
}

TEST_CASE("MigrationRunner: current_version returns value from version_query",
          "[infra][migrations]") {
    FakeExecutor exec;
    FakeVersionQuery vq{42u};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    auto v = runner.current_version();
    REQUIRE(v.has_value());
    REQUIRE(v.value() == 42u);
}

// ---------------------------------------------------------------------------
// migrate_to_latest — empty migrations
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: migrate_to_latest with empty span returns 0",
          "[infra][migrations]") {
    FakeExecutor exec;
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    auto result = runner.migrate_to_latest({});
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0u);
    // No SQL should have been executed
    REQUIRE(exec.executed.empty());
}

// ---------------------------------------------------------------------------
// migrate_to_latest — fresh database (no prior version)
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: migrate_to_latest applies all migrations on fresh db",
          "[infra][migrations]") {
    FakeExecutor exec;
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    const std::array<Migration, 2> migs{kMig1, kMig2};
    auto result = runner.migrate_to_latest(migs);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 2u);

    // Verify both migration SQL strings were executed
    bool found1 = false, found2 = false;
    for (const auto& sql : exec.executed) {
        if (sql.find("CREATE TABLE accounts") != std::string::npos) found1 = true;
        if (sql.find("ALTER TABLE accounts") != std::string::npos)  found2 = true;
    }
    REQUIRE(found1);
    REQUIRE(found2);
}

// ---------------------------------------------------------------------------
// migrate_to_latest — already at latest
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: migrate_to_latest skips already-applied migrations",
          "[infra][migrations]") {
    FakeExecutor exec;
    // Pretend version 2 is already applied
    FakeVersionQuery vq{2u};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    const std::array<Migration, 2> migs{kMig1, kMig2};
    auto result = runner.migrate_to_latest(migs);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 2u);

    // Neither migration SQL should have been executed (only ensure_migration_table)
    bool found1 = false, found2 = false;
    for (const auto& sql : exec.executed) {
        if (sql.find("CREATE TABLE accounts") != std::string::npos) found1 = true;
        if (sql.find("ALTER TABLE accounts") != std::string::npos)  found2 = true;
    }
    REQUIRE_FALSE(found1);
    REQUIRE_FALSE(found2);
}

// ---------------------------------------------------------------------------
// migrate_to — partial migration
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: migrate_to applies only up to target version",
          "[infra][migrations]") {
    FakeExecutor exec;
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    const std::array<Migration, 2> migs{kMig1, kMig2};
    // Migrate only to version 1
    auto result = runner.migrate_to(migs, 1u);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 1u);

    bool found1 = false, found2 = false;
    for (const auto& sql : exec.executed) {
        if (sql.find("CREATE TABLE accounts") != std::string::npos) found1 = true;
        if (sql.find("ALTER TABLE accounts") != std::string::npos)  found2 = true;
    }
    REQUIRE(found1);
    REQUIRE_FALSE(found2);
}

// ---------------------------------------------------------------------------
// migrate_to — unsorted migrations rejected
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: migrate_to rejects unsorted migrations",
          "[infra][migrations]") {
    FakeExecutor exec;
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    // Deliberately reversed order
    const std::array<Migration, 2> migs{kMig2, kMig1};
    auto result = runner.migrate_to_latest(migs);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// migrate_to — executor failure propagated
// ---------------------------------------------------------------------------

TEST_CASE("MigrationRunner: migrate_to propagates executor failure",
          "[infra][migrations]") {
    FakeExecutor exec;
    exec.fail_on = "CREATE TABLE accounts";
    FakeVersionQuery vq{std::nullopt};

    MigrationRunner runner(
        [&](std::string_view sql) { return exec(sql); },
        [&]() { return vq(); });

    const std::array<Migration, 1> migs{kMig1};
    auto result = runner.migrate_to_latest(migs);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::Internal);
}

}  // namespace pvpgn::infra::migrations
