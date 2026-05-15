// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection.hpp
/// MySQL/MariaDB database connection wrapper with transaction support.
///
/// Provides synchronous access to MySQL/MariaDB databases using libmysqlclient
/// or connector-cpp. Gated behind PVPGN_V3_WITH_MYSQL CMake option.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

#ifdef PVPGN_V3_WITH_MYSQL

namespace pvpgn::infra::mysql {

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
    friend class MySQLConnection;
    explicit Row(void* row, void* metadata) : row_(row), metadata_(metadata) {}
    void* row_;
    void* metadata_;
};

/// MySQL/MariaDB database connection wrapper.
/// Manages connection lifecycle, transactions, and query execution.
class MySQLConnection {
public:
    /// Type alias for row callback function.
    /// Return false to stop iteration, true to continue.
    using RowCallback = std::function<bool(const Row&)>;

    /// Create a connection to a MySQL/MariaDB database.
    /// @param host Database server hostname or IP
    /// @param port Database server port (default 3306)
    /// @param user Authentication username
    /// @param password Authentication password
    /// @param database Initial database to select
    explicit MySQLConnection(std::string_view host, std::uint16_t port,
                             std::string_view user, std::string_view password,
                             std::string_view database);

    ~MySQLConnection();

    // Non-copyable
    MySQLConnection(const MySQLConnection&) = delete;
    MySQLConnection& operator=(const MySQLConnection&) = delete;

    // Movable
    MySQLConnection(MySQLConnection&& other) noexcept;
    MySQLConnection& operator=(MySQLConnection&& other) noexcept;

    /// Execute SQL that returns no rows (DDL, INSERT, UPDATE, DELETE).
    /// @param sql SQL statement to execute
    /// @return Error if execution fails
    core::Result<void, core::Error> exec(std::string_view sql);

    /// Execute a query with callback for each row.
    /// @param sql SQL statement
    /// @param cb Callback function called for each row
    /// @return Error if execution fails
    core::Result<void, core::Error> query(std::string_view sql, const RowCallback& cb);

    /// Get the ID of the last inserted row (auto_increment).
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
    /// @return MySQLConnection if successful, Error otherwise
    static core::Result<MySQLConnection, core::Error> from_connection_string(
        std::string_view cs, std::string_view user, std::string_view password);

private:
    struct Impl;
    void* handle_;  // MYSQL* opaque handle
    bool in_transaction_;

    void cleanup() noexcept;
};

}  // namespace pvpgn::infra::mysql

#endif  // PVPGN_V3_WITH_MYSQL
