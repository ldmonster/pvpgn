// SPDX-License-Identifier: GPL-2.0-or-later

/// @file connection.cpp
/// MySQL/MariaDB database connection wrapper implementation.
///
/// Compiled only when PVPGN_V3_WITH_MYSQL is defined (libmysqlclient present).
/// When MySQL is not available, this translation unit is effectively empty
/// because the header guards all class declarations behind the same macro.

#include "infra/mysql/connection.hpp"

#ifdef PVPGN_V3_WITH_MYSQL

#include <mysql/mysql.h>

#include <stdexcept>
#include <string>

namespace pvpgn::infra::mysql {

// ---------------------------------------------------------------------------
// Row implementation
// ---------------------------------------------------------------------------

std::string Row::get_text(std::size_t col) const {
    auto* row = static_cast<MYSQL_ROW>(row_);
    auto* meta = static_cast<MYSQL_RES*>(metadata_);
    const unsigned long* lengths = mysql_fetch_lengths(meta);
    if (!row || !row[col]) return {};
    return std::string(row[col], lengths ? lengths[col] : std::strlen(row[col]));
}

std::int64_t Row::get_int(std::size_t col) const {
    auto* row = static_cast<MYSQL_ROW>(row_);
    if (!row || !row[col]) return 0;
    return static_cast<std::int64_t>(std::stoll(row[col]));
}

bool Row::is_null(std::size_t col) const {
    auto* row = static_cast<MYSQL_ROW>(row_);
    return !row || row[col] == nullptr;
}

std::size_t Row::column_count() const {
    auto* meta = static_cast<MYSQL_RES*>(metadata_);
    if (!meta) return 0;
    return static_cast<std::size_t>(mysql_num_fields(meta));
}

// ---------------------------------------------------------------------------
// MySQLConnection implementation
// ---------------------------------------------------------------------------

MySQLConnection::MySQLConnection(std::string_view host, std::uint16_t port,
                                 std::string_view user, std::string_view password,
                                 std::string_view database)
    : handle_(nullptr), in_transaction_(false) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        throw std::runtime_error("mysql_init failed: out of memory");
    }

    const unsigned int port_u = static_cast<unsigned int>(port);
    if (!mysql_real_connect(conn,
                            std::string(host).c_str(),
                            std::string(user).c_str(),
                            std::string(password).c_str(),
                            std::string(database).c_str(),
                            port_u,
                            nullptr,
                            CLIENT_MULTI_STATEMENTS)) {
        const std::string err = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("mysql_real_connect failed: " + err);
    }

    // Enable auto-reconnect
    my_bool reconnect = 1;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

    handle_ = conn;
}

MySQLConnection::~MySQLConnection() {
    cleanup();
}

MySQLConnection::MySQLConnection(MySQLConnection&& other) noexcept
    : handle_(other.handle_), in_transaction_(other.in_transaction_) {
    other.handle_ = nullptr;
    other.in_transaction_ = false;
}

MySQLConnection& MySQLConnection::operator=(MySQLConnection&& other) noexcept {
    if (this != &other) {
        cleanup();
        handle_ = other.handle_;
        in_transaction_ = other.in_transaction_;
        other.handle_ = nullptr;
        other.in_transaction_ = false;
    }
    return *this;
}

void MySQLConnection::cleanup() noexcept {
    if (handle_) {
        mysql_close(static_cast<MYSQL*>(handle_));
        handle_ = nullptr;
    }
}

core::Result<void, core::Error> MySQLConnection::exec(std::string_view sql) {
    if (!handle_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "mysql: connection not open"});
    }
    auto* conn = static_cast<MYSQL*>(handle_);
    if (mysql_real_query(conn, sql.data(), static_cast<unsigned long>(sql.size())) != 0) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      std::string("mysql exec: ") + mysql_error(conn)});
    }
    // Consume any result sets (for multi-statement safety)
    MYSQL_RES* res = mysql_store_result(conn);
    if (res) mysql_free_result(res);
    return core::ok();
}

core::Result<void, core::Error> MySQLConnection::query(std::string_view sql,
                                                        const RowCallback& cb) {
    if (!handle_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "mysql: connection not open"});
    }
    auto* conn = static_cast<MYSQL*>(handle_);
    if (mysql_real_query(conn, sql.data(), static_cast<unsigned long>(sql.size())) != 0) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      std::string("mysql query: ") + mysql_error(conn)});
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        // Query returned no result set (e.g. INSERT/UPDATE) — not an error
        return core::ok();
    }

    MYSQL_ROW mysql_row;
    while ((mysql_row = mysql_fetch_row(res)) != nullptr) {
        Row row(static_cast<void*>(mysql_row), static_cast<void*>(res));
        if (!cb(row)) break;
    }
    mysql_free_result(res);
    return core::ok();
}

std::int64_t MySQLConnection::last_insert_id() const {
    if (!handle_) return 0;
    return static_cast<std::int64_t>(
        mysql_insert_id(static_cast<MYSQL*>(handle_)));
}

core::Result<void, core::Error> MySQLConnection::begin() {
    if (in_transaction_) return core::ok();
    auto r = exec("START TRANSACTION");
    if (r.has_value()) in_transaction_ = true;
    return r;
}

core::Result<void, core::Error> MySQLConnection::commit() {
    if (!in_transaction_) return core::ok();
    auto r = exec("COMMIT");
    if (r.has_value()) in_transaction_ = false;
    return r;
}

void MySQLConnection::rollback() noexcept {
    if (!in_transaction_ || !handle_) return;
    mysql_query(static_cast<MYSQL*>(handle_), "ROLLBACK");
    in_transaction_ = false;
}

core::Result<MySQLConnection, core::Error>
MySQLConnection::from_connection_string(std::string_view cs,
                                        std::string_view user,
                                        std::string_view password) {
    // Expected format: "host:port/database"
    const std::string s{cs};
    const auto colon = s.find(':');
    const auto slash = s.find('/');
    if (colon == std::string::npos || slash == std::string::npos || slash <= colon) {
        return core::fail(core::Error{core::StatusCode::InvalidArgument,
                                      "mysql: invalid connection string (expected host:port/db)"});
    }
    const std::string host = s.substr(0, colon);
    const std::uint16_t port = static_cast<std::uint16_t>(
        std::stoul(s.substr(colon + 1, slash - colon - 1)));
    const std::string database = s.substr(slash + 1);

    try {
        return MySQLConnection(host, port, user, password, database);
    } catch (const std::exception& ex) {
        return core::fail(core::Error{core::StatusCode::Internal, ex.what()});
    }
}

}  // namespace pvpgn::infra::mysql

#endif  // PVPGN_V3_WITH_MYSQL
