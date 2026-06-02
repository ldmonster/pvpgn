// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/connection.hpp"

#include <sqlite3.h>

namespace pvpgn::infra::sqlite {

// Row implementation
std::string Row::get_text(int col) const {
    const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
    return val ? std::string{val} : std::string{};
}

std::int64_t Row::get_int(int col) const {
    return sqlite3_column_int64(stmt_, col);
}

std::span<const std::byte> Row::get_blob(int col) const {
    const void* data = sqlite3_column_blob(stmt_, col);
    int size = sqlite3_column_bytes(stmt_, col);
    return std::span<const std::byte>{static_cast<const std::byte*>(data), static_cast<std::size_t>(size)};
}

bool Row::is_null(int col) const {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

int Row::column_count() const {
    return sqlite3_column_count(stmt_);
}

// SQLiteConnection implementation
SQLiteConnection::SQLiteConnection(std::string_view path)
    : db_(nullptr), in_transaction_(false) {
    int rc = sqlite3_open(std::string{path}.c_str(), &db_);
    if (rc != SQLITE_OK) {
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
    }
}

SQLiteConnection::~SQLiteConnection() {
    if (db_) {
        if (in_transaction_) {
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        }
        sqlite3_close(db_);
    }
}

SQLiteConnection::SQLiteConnection(SQLiteConnection&& other) noexcept
    : db_(other.db_), in_transaction_(other.in_transaction_) {
    other.db_ = nullptr;
    other.in_transaction_ = false;
}

SQLiteConnection& SQLiteConnection::operator=(SQLiteConnection&& other) noexcept {
    if (this != &other) {
        if (db_) {
            if (in_transaction_) {
                sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            }
            sqlite3_close(db_);
        }
        db_ = other.db_;
        in_transaction_ = other.in_transaction_;
        other.db_ = nullptr;
        other.in_transaction_ = false;
    }
    return *this;
}

core::Result<void, core::Error> SQLiteConnection::exec(std::string_view sql) {
    if (!db_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not open"});
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, std::string{sql}.c_str(), nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? std::string{err_msg} : "unknown error";
        sqlite3_free(err_msg);
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string{"sqlite: "} + msg});
    }

    return core::ok();
}

core::Result<void, core::Error> SQLiteConnection::query(std::string_view sql, const RowCallback& cb) {
    if (!db_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not open"});
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, std::string{sql}.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string{"sqlite: prepare failed: "} + sqlite3_errmsg(db_)});
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row{stmt};
        if (!cb(row)) {
            break;
        }
    }

    sqlite3_finalize(stmt);
    return core::ok();
}

core::Result<void, core::Error> SQLiteConnection::query_bind(
    std::string_view sql,
    std::initializer_list<ParamValue> params,
    const RowCallback& cb) {
    return query_bind(sql,
                      std::span<const ParamValue>{params.begin(), params.size()},
                      cb);
}

core::Result<void, core::Error> SQLiteConnection::query_bind(
    std::string_view sql,
    std::span<const ParamValue> params,
    const RowCallback& cb) {
    if (!db_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not open"});
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, std::string{sql}.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string{"sqlite: prepare failed: "} + sqlite3_errmsg(db_)});
    }

    // Bind parameters
    int param_idx = 1;
    for (const auto& param : params) {
        std::visit(
            [&](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, std::int64_t>) {
                    sqlite3_bind_int64(stmt, param_idx, val);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    sqlite3_bind_text(stmt, param_idx, val.c_str(), -1, SQLITE_TRANSIENT);
                } else if constexpr (std::is_same_v<T, std::vector<std::byte>>) {
                    sqlite3_bind_blob(stmt, param_idx, val.data(), static_cast<int>(val.size()), SQLITE_TRANSIENT);
                } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    sqlite3_bind_null(stmt, param_idx);
                }
            },
            param);
        ++param_idx;
    }

    // Execute query
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row{stmt};
        if (!cb(row)) {
            break;
        }
    }

    sqlite3_finalize(stmt);
    return core::ok();
}

std::int64_t SQLiteConnection::last_insert_rowid() const {
    return db_ ? sqlite3_last_insert_rowid(db_) : 0;
}

core::Result<void, core::Error> SQLiteConnection::begin() {
    if (!db_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not open"});
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? std::string{err_msg} : "unknown error";
        sqlite3_free(err_msg);
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string{"sqlite: begin failed: "} + msg});
    }

    in_transaction_ = true;
    return core::ok();
}

core::Result<void, core::Error> SQLiteConnection::commit() {
    if (!db_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not open"});
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? std::string{err_msg} : "unknown error";
        sqlite3_free(err_msg);
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string{"sqlite: commit failed: "} + msg});
    }

    in_transaction_ = false;
    return core::ok();
}

void SQLiteConnection::rollback() noexcept {
    if (db_ && in_transaction_) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        in_transaction_ = false;
    }
}

}  // namespace pvpgn::infra::sqlite
