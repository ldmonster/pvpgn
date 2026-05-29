// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file migration_runner.hpp
/// Manages application of database schema migrations.
///
/// The MigrationRunner tracks applied migrations in a dedicated table
/// and applies pending migrations in order, ensuring idempotency and
/// atomic application of schema changes.

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::migrations {

/// A single database migration to be applied atomically.
struct Migration {
    std::uint32_t version;          ///< Monotonically increasing version (1, 2, 3, ...)
    std::string_view name;          ///< Human-readable name (e.g., "001_initial_schema")
    std::string_view up_sql;        ///< SQL to apply when moving forward
    std::string_view down_sql;      ///< SQL to rollback (optional, can be empty)
};

/// Executes database migrations, tracking applied versions.
/// Thread-safe migration execution: ensures each migration runs atomically.
class MigrationRunner {
public:
    /// Type alias for SQL execution function
    /// The executor function receives raw SQL and must handle execution.
    /// It should return an error if execution fails.
    using SqlExecutor = std::function<core::Result<void, core::Error>(std::string_view sql)>;

    /// Type alias for version query function
    /// Returns the currently applied migration version, or std::nullopt if none.
    using VersionQuery = std::function<std::optional<std::uint32_t>()>;

    /// Create a migration runner with custom SQL executor and version query.
    /// @param executor Function to execute SQL statements
    /// @param version_query Function to get current schema version
    explicit MigrationRunner(SqlExecutor executor, VersionQuery version_query);

    /// Apply all pending migrations up to the latest version.
    /// @param migrations Span of migrations to consider (should be sorted by version)
    /// @return Applied version on success, or Error on failure
    core::Result<std::uint32_t, core::Error> migrate_to_latest(
        std::span<const Migration> migrations);

    /// Apply migrations up to a specific target version.
    /// @param migrations Span of migrations to consider
    /// @param target Target version to migrate to
    /// @return Applied version on success, or Error on failure
    core::Result<std::uint32_t, core::Error> migrate_to(
        std::span<const Migration> migrations, std::uint32_t target);

    /// Get the currently applied schema version.
    /// @return Current version, or std::nullopt if migrations table doesn't exist
    std::optional<std::uint32_t> current_version() const;

    /// Ensure the migration tracking table exists.
    /// Creates `_schema_migrations` if it doesn't exist.
    /// @return Error if creation fails
    core::Result<void, core::Error> ensure_migration_table();

private:
    /// Mark a migration as applied in the tracking table.
    core::Result<void, core::Error> record_migration(
        std::uint32_t version, std::string_view name, std::string_view checksum);

    /// Compute a simple checksum of migration SQL for integrity verification.
    static std::string compute_checksum(std::string_view sql);

    SqlExecutor executor_;
    VersionQuery version_query_;
};

}  // namespace pvpgn::infra::migrations
