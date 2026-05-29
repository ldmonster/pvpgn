// SPDX-License-Identifier: GPL-2.0-or-later

/// @file connection.cpp
/// PostgreSQL database connection wrapper implementation.
///
/// Compiled only when PVPGN_V3_WITH_POSTGRESQL is defined (libpq present).
/// When PostgreSQL is not available, this translation unit is effectively empty
/// because the header guards all class declarations behind the same macro.

#include "infra/postgres/connection.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

#include <libpq-fe.h>

#include <stdexcept>
#include <string>

namespace pvpgn::infra::postgres {

// ---------------------------------------------------------------------------
// Row implementation
// ---------------------------------------------------------------------------

std::string Row::get_text(std::size_t col) const {
    auto* res = static_cast<PGresult*>(result_);
    if (!res) return {};
    if (PQgetisnull(res, row_num_, static_cast<int>(col))) return {};
    const char* val = PQgetvalue(res, row_num_, static_cast<int>(col));
    return val ? std::string(val) : std::string{};
}

std::int64_t Row::get_int(std::size_t col) const {
    const std::string text = get_text(col);
    if (text.empty()) return 0;
    try {
        return static_cast<std::int64_t>(std::stoll(text));
    } catch (...) {
        return 0;
    }
}

bool Row::is_null(std::size_t col) const {
    auto* res = static_cast<PGresult*>(result_);
    if (!res) return true;
    return PQgetisnull(res, row_num_, static_cast<int>(col)) != 0;
}

std::size_t Row::column_count() const {
    auto* res = static_cast<PGresult*>(result_);
    if (!res) return 0;
    return static_cast<std::size_t>(PQnfields(res));
}

// ---------------------------------------------------------------------------
// PostgreSQLConnection implementation
// ---------------------------------------------------------------------------

PostgreSQLConnection::PostgreSQLConnection(std::string_view host, std::uint16_t port,
                                           std::string_view user, std::string_view password,
                                           std::string_view database)
    : handle_(nullptr), in_transaction_(false) {
    // Build a libpq connection string
    const std::string conninfo =
        "host=" + std::string(host) +
        " port=" + std::to_string(port) +
        " user=" + std::string(user) +
        " password=" + std::string(password) +
        " dbname=" + std::string(database);

    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (!conn) {
        throw std::runtime_error("PQconnectdb: out of memory");
    }
    if (PQstatus(conn) != CONNECTION_OK) {
        const std::string err = PQerrorMessage(conn);
        PQfinish(conn);
        throw std::runtime_error("PostgreSQL connection failed: " + err);
    }
    handle_ = conn;
}

PostgreSQLConnection::~PostgreSQLConnection() {
    cleanup();
}

PostgreSQLConnection::PostgreSQLConnection(PostgreSQLConnection&& other) noexcept
    : handle_(other.handle_), in_transaction_(other.in_transaction_) {
    other.handle_ = nullptr;
    other.in_transaction_ = false;
}

PostgreSQLConnection& PostgreSQLConnection::operator=(PostgreSQLConnection&& other) noexcept {
    if (this != &other) {
        cleanup();
        handle_ = other.handle_;
        in_transaction_ = other.in_transaction_;
        other.handle_ = nullptr;
        other.in_transaction_ = false;
    }
    return *this;
}

void PostgreSQLConnection::cleanup() noexcept {
    if (handle_) {
        PQfinish(static_cast<PGconn*>(handle_));
        handle_ = nullptr;
    }
}

core::Result<void, core::Error> PostgreSQLConnection::exec(std::string_view sql) {
    if (!handle_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "postgres: connection not open"});
    }
    auto* conn = static_cast<PGconn*>(handle_);
    PGresult* res = PQexec(conn, std::string(sql).c_str());
    if (!res) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "postgres exec: PQexec returned null"});
    }
    const ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        const std::string err = PQresultErrorMessage(res);
        PQclear(res);
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "postgres exec: " + err});
    }
    PQclear(res);
    return {};
}

core::Result<void, core::Error> PostgreSQLConnection::query(
    std::string_view sql, const RowCallback& cb) {
    if (!handle_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "postgres: connection not open"});
    }
    auto* conn = static_cast<PGconn*>(handle_);
    PGresult* res = PQexec(conn, std::string(sql).c_str());
    if (!res) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "postgres query: PQexec returned null"});
    }
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const std::string err = PQresultErrorMessage(res);
        PQclear(res);
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "postgres query: " + err});
    }

    const int nrows = PQntuples(res);
    for (int i = 0; i < nrows; ++i) {
        Row row(res, i);
        if (!cb(row)) break;
    }
    PQclear(res);
    return {};
}

std::int64_t PostgreSQLConnection::last_insert_id() const {
    // PostgreSQL uses RETURNING clause or currval(); this is a placeholder.
    // Callers should use RETURNING id in their INSERT statements.
    return 0;
}

core::Result<void, core::Error> PostgreSQLConnection::begin() {
    auto r = exec("BEGIN");
    if (r.has_value()) in_transaction_ = true;
    return r;
}

core::Result<void, core::Error> PostgreSQLConnection::commit() {
    auto r = exec("COMMIT");
    in_transaction_ = false;
    return r;
}

void PostgreSQLConnection::rollback() noexcept {
    if (in_transaction_) {
        (void)exec("ROLLBACK");
        in_transaction_ = false;
    }
}

core::Result<PostgreSQLConnection, core::Error>
PostgreSQLConnection::from_connection_string(
    std::string_view cs, std::string_view user, std::string_view password) {
    // Parse "host:port/database"
    std::string host = "localhost";
    std::uint16_t port = 5432;
    std::string database = "pvpgn";

    const auto slash_pos = cs.find('/');
    std::string_view host_port = (slash_pos != std::string_view::npos)
                                     ? cs.substr(0, slash_pos)
                                     : cs;
    if (slash_pos != std::string_view::npos) {
        database = std::string(cs.substr(slash_pos + 1));
    }

    const auto colon_pos = host_port.find(':');
    if (colon_pos != std::string_view::npos) {
        host = std::string(host_port.substr(0, colon_pos));
        try {
            port = static_cast<std::uint16_t>(
                std::stoul(std::string(host_port.substr(colon_pos + 1))));
        } catch (...) {}
    } else {
        host = std::string(host_port);
    }

    try {
        return PostgreSQLConnection(host, port, user, password, database);
    } catch (const std::exception& ex) {
        return core::fail(core::Error{core::StatusCode::Internal, ex.what()});
    }
}

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
