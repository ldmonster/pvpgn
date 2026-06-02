// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection.hpp
/// SQLite database connection wrapper with transaction support.
///
/// Provides synchronous, single-threaded access to SQLite databases.
/// For multi-threaded access, create one connection per thread.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

// Forward declare SQLite types
struct sqlite3;
struct sqlite3_stmt;

namespace pvpgn::infra::sqlite {

/// Represents a single row returned from a query.
class Row {
public:
    /// Get text value from column (returns empty string if NULL).
    std::string get_text(int col) const;

    /// Get integer value from column (returns 0 if NULL).
    std::int64_t get_int(int col) const;

    /// Get BLOB data from column.
    std::span<const std::byte> get_blob(int col) const;

    /// Check if column is NULL.
    bool is_null(int col) const;

    /// Get number of columns in this row.
    int column_count() const;

private:
    friend class SQLiteConnection;
    explicit Row(sqlite3_stmt* stmt) : stmt_(stmt) {}
    sqlite3_stmt* stmt_;
};

/// SQLite database connection wrapper.
/// Manages connection lifecycle, transactions, and query execution.
class SQLiteConnection {
public:
    /// Type alias for row callback function.
    /// Return false to stop iteration, true to continue.
    using RowCallback = std::function<bool(const Row&)>;

    /// Type alias for parameter variant (supports multiple types).
    using ParamValue = std::variant<std::int64_t, std::string, std::vector<std::byte>, std::nullptr_t>;

    /// Create a connection to a SQLite database.
    /// @param path Database file path, or ":memory:" for in-memory DB
    explicit SQLiteConnection(std::string_view path);

    ~SQLiteConnection();

    // Non-copyable
    SQLiteConnection(const SQLiteConnection&) = delete;
    SQLiteConnection& operator=(const SQLiteConnection&) = delete;

    // Movable
    SQLiteConnection(SQLiteConnection&& other) noexcept;
    SQLiteConnection& operator=(SQLiteConnection&& other) noexcept;

    /// Execute SQL that returns no rows (DDL, INSERT, UPDATE, DELETE).
    /// @param sql SQL statement to execute
    /// @return Error if execution fails
    core::Result<void, core::Error> exec(std::string_view sql);

    /// Execute a query with optional parameters.
    /// @param sql SQL statement with ? placeholders for parameters
    /// @param cb Callback function called for each row
    /// @return Error if execution fails
    core::Result<void, core::Error> query(std::string_view sql, const RowCallback& cb);

    /// Execute a parameterized query.
    /// @param sql SQL statement with ? placeholders
    /// @param params Parameter values to bind (in order)
    /// @param cb Callback function called for each row
    /// @return Error if execution fails
    core::Result<void, core::Error> query_bind(
        std::string_view sql,
        std::initializer_list<ParamValue> params,
        const RowCallback& cb);

    /// Overload taking a runtime-built parameter range (e.g. from the
    /// `IDbDriver` adapter, which can't use a compile-time initializer_list).
    core::Result<void, core::Error> query_bind(
        std::string_view sql,
        std::span<const ParamValue> params,
        const RowCallback& cb);

    /// Get the rowid of the last inserted row.
    std::int64_t last_insert_rowid() const;

    /// Begin a transaction.
    /// @return Error if transaction start fails
    core::Result<void, core::Error> begin();

    /// Commit the current transaction.
    /// @return Error if commit fails
    core::Result<void, core::Error> commit();

    /// Rollback the current transaction.
    void rollback() noexcept;

    /// Check if a connection is currently open.
    bool is_open() const { return db_ != nullptr; }

private:
    sqlite3* db_;
    bool in_transaction_;
};

}  // namespace pvpgn::infra::sqlite
