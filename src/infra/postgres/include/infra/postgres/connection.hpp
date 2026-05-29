// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection.hpp
/// PostgreSQL database connection wrapper with transaction support.
///
/// Provides synchronous access to PostgreSQL databases using libpqxx or libpq.
/// Gated behind PVPGN_V3_WITH_POSTGRESQL CMake option.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

namespace pvpgn::infra::postgres {

/// Represents a single row returned from a query.
class Row {
public:
    /// Get text value from column (returns empty string if NULL).
    std::string get_text(std::size_t col) const;

    /// Get integer value from column (returns 0 if NULL).
    std::int64_t get_int(std::size_t col) const;

    /// Check if column is NULL.
    bool is_null(std::size_t col) const;

    /// Get number of columns in this row.
    std::size_t column_count() const;

private:
    friend class PostgreSQLConnection;
    explicit Row(void* result, int row_num) : result_(result), row_num_(row_num) {}
    void* result_;  // PGresult* opaque handle
    int row_num_;
};

/// PostgreSQL database connection wrapper.
/// Manages connection lifecycle, transactions, and query execution.
class PostgreSQLConnection {
public:
    /// Type alias for row callback function.
    /// Return false to stop iteration, true to continue.
    using RowCallback = std::function<bool(const Row&)>;

    /// Create a connection to a PostgreSQL database.
    /// @param host Database server hostname or IP
    /// @param port Database server port (default 5432)
    /// @param user Authentication username
    /// @param password Authentication password
    /// @param database Initial database to select
    explicit PostgreSQLConnection(std::string_view host, std::uint16_t port,
                                  std::string_view user, std::string_view password,
                                  std::string_view database);

    ~PostgreSQLConnection();

    // Non-copyable
    PostgreSQLConnection(const PostgreSQLConnection&) = delete;
    PostgreSQLConnection& operator=(const PostgreSQLConnection&) = delete;

    // Movable
    PostgreSQLConnection(PostgreSQLConnection&& other) noexcept;
    PostgreSQLConnection& operator=(PostgreSQLConnection&& other) noexcept;

    /// Execute SQL that returns no rows (DDL, INSERT, UPDATE, DELETE).
    /// @param sql SQL statement to execute
    /// @return Error if execution fails
    core::Result<void, core::Error> exec(std::string_view sql);

    /// Execute a query with callback for each row.
    /// @param sql SQL statement (uses $1, $2, ... for parameters)
    /// @param cb Callback function called for each row
    /// @return Error if execution fails
    core::Result<void, core::Error> query(std::string_view sql, const RowCallback& cb);

    /// Get the ID of the last inserted row (currval on SERIAL sequence).
    std::int64_t last_insert_id() const;

    /// Begin a transaction.
    /// @return Error if transaction start fails
    core::Result<void, core::Error> begin();

    /// Commit the current transaction.
    /// @return Error if commit fails
    core::Result<void, core::Error> commit();

    /// Rollback the current transaction.
    void rollback() noexcept;

    /// Check if a connection is currently open.
    bool is_open() const { return handle_ != nullptr; }

    /// Parse "host:port/database" style connection string
    /// and create a connection.
    /// @param cs Connection string in format "host:port/database"
    /// @param user Username
    /// @param password Password
    /// @return PostgreSQLConnection if successful, Error otherwise
    static core::Result<PostgreSQLConnection, core::Error> from_connection_string(
        std::string_view cs, std::string_view user, std::string_view password);

private:
    void* handle_;  // PGconn* opaque handle
    bool in_transaction_;

    void cleanup() noexcept;
};

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
