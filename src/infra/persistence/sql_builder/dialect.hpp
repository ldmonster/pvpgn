// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file dialect.hpp
/// SQL dialect abstraction for multi-backend support.
///
/// Provides dialect-specific SQL generation helpers for SQLite, MySQL, and PostgreSQL.

#include <string>
#include <string_view>

namespace pvpgn::infra::persistence {

/// Supported SQL dialects.
enum class SqlDialect {
    SQLite,
    MySQL,
    Postgres,
};

/// SQL dialect utilities for generating dialect-specific SQL.
class SqlDialectHelper {
public:
    explicit SqlDialectHelper(SqlDialect dialect) : dialect_(dialect) {}

    /// Get the parameter placeholder for this dialect.
    /// SQLite: "?"
    /// MySQL: "?"
    /// PostgreSQL: "$1", "$2", etc. (caller must track position)
    std::string_view placeholder() const;

    /// Get the parameter placeholder for a specific position (1-indexed).
    /// Only meaningful for PostgreSQL; others return "?".
    std::string placeholder_at(int position) const;

    /// Get the UPSERT syntax for this dialect.
    /// SQLite: "INSERT OR REPLACE INTO table ..."
    /// MySQL: "INSERT INTO table ... ON DUPLICATE KEY UPDATE ..."
    /// PostgreSQL: "INSERT INTO table ... ON CONFLICT ... DO UPDATE SET ..."
    std::string_view upsert_prefix() const;

    /// Get the quote character for identifiers.
    /// SQLite: '"'
    /// MySQL: '`'
    /// PostgreSQL: '"'
    char quote_char() const;

    /// Quote an identifier (table name, column name, etc.).
    std::string quote_identifier(std::string_view name) const;

    /// Get the RETURNING clause syntax (if supported).
    /// SQLite: "" (not supported, use last_insert_rowid())
    /// MySQL: "" (not supported)
    /// PostgreSQL: "RETURNING ..."
    std::string_view returning_prefix() const;

    /// Check if this dialect supports RETURNING.
    bool supports_returning() const;

    /// Get the dialect.
    SqlDialect dialect() const { return dialect_; }

private:
    SqlDialect dialect_;
};

}  // namespace pvpgn::infra::persistence
