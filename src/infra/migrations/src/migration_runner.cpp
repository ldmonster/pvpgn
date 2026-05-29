// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/migrations/migration_runner.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace pvpgn::infra::migrations {

MigrationRunner::MigrationRunner(SqlExecutor executor, VersionQuery version_query)
    : executor_(std::move(executor)), version_query_(std::move(version_query)) {}

core::Result<void, core::Error> MigrationRunner::ensure_migration_table() {
    constexpr std::string_view create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS _schema_migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            applied_at INTEGER NOT NULL,
            checksum TEXT NOT NULL
        );
    )";
    return executor_(create_table_sql);
}

std::optional<std::uint32_t> MigrationRunner::current_version() const {
    return version_query_();
}

namespace {

/// CRC32 lookup table (IEEE 802.3 polynomial 0xEDB88320, reflected).
constexpr std::array<std::uint32_t, 256> make_crc32_table() noexcept {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
        }
        table[i] = crc;
    }
    return table;
}

constexpr auto kCrc32Table = make_crc32_table();

/// Compute CRC32 over UTF-8 bytes of `data`.
std::uint32_t crc32(std::string_view data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char byte : data) {
        crc = kCrc32Table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

}  // namespace

std::string MigrationRunner::compute_checksum(std::string_view sql) {
    // CRC32 over the migration SQL text (UTF-8 bytes), formatted as 8 hex digits.
    const std::uint32_t value = crc32(sql);
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << value;
    return oss.str();
}

core::Result<void, core::Error> MigrationRunner::record_migration(
    std::uint32_t version, std::string_view name, std::string_view checksum) {
    // Record the migration in the tracking table
    // Using parameterized query syntax (implementation-specific)
    auto now = std::chrono::system_clock::now().time_since_epoch().count();

    std::ostringstream sql;
    sql << "INSERT INTO _schema_migrations (version, name, applied_at, checksum) "
        << "VALUES (" << version << ", '" << name << "', " << now << ", '"
        << checksum << "');";

    return executor_(sql.str());
}

core::Result<std::uint32_t, core::Error> MigrationRunner::migrate_to_latest(
    std::span<const Migration> migrations) {
    if (migrations.empty()) {
        return 0;  // No migrations to apply
    }

    // Find the latest version
    auto latest_it =
        std::max_element(migrations.begin(), migrations.end(),
                         [](const Migration& a, const Migration& b) {
                             return a.version < b.version;
                         });

    if (latest_it == migrations.end()) {
        return 0;
    }

    return migrate_to(migrations, latest_it->version);
}

core::Result<std::uint32_t, core::Error> MigrationRunner::migrate_to(
    std::span<const Migration> migrations, std::uint32_t target) {
    // Ensure migration table exists
    if (auto result = ensure_migration_table(); !result.has_value()) {
        return core::fail(result.error());
    }

    // Get current version
    auto current = current_version().value_or(0);

    // Validate that migrations are sorted
    for (std::size_t i = 1; i < migrations.size(); ++i) {
        if (migrations[i].version <= migrations[i - 1].version) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "migrations: not sorted by version"});
        }
    }

    // Apply migrations in order up to target
    for (const auto& migration : migrations) {
        if (migration.version <= current) {
            continue;  // Already applied
        }
        if (migration.version > target) {
            break;  // Stop at target
        }

        // Apply the migration
        if (auto result = executor_(migration.up_sql); !result.has_value()) {
            return core::fail(core::Error{
                core::StatusCode::Internal,
                std::string("migration ") + std::to_string(migration.version) +
                    " failed: " + result.error().message()});
        }

        // Record the migration
        auto checksum = compute_checksum(migration.up_sql);
        if (auto result = record_migration(migration.version, migration.name, checksum);
            !result.has_value()) {
            return core::fail(core::Error{
                core::StatusCode::Internal,
                std::string("failed to record migration ") +
                    std::to_string(migration.version)});
        }

        current = migration.version;
    }

    return current;
}

}  // namespace pvpgn::infra::migrations
