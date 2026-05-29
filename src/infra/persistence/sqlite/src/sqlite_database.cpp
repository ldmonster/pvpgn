// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/sqlite/sqlite_database.hpp"
#include "core/error.hpp"
#include <sqlite3.h>
#include <utility>

namespace pvpgn::infra::persistence::sqlite {

// ============================================================================
// SqliteStatement
// ============================================================================

SqliteStatement::SqliteStatement(sqlite3_stmt* stmt) : stmt_(stmt) {}

SqliteStatement::~SqliteStatement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
    }
}

SqliteStatement::SqliteStatement(SqliteStatement&& other) noexcept
    : stmt_(std::exchange(other.stmt_, nullptr)) {}

SqliteStatement& SqliteStatement::operator=(SqliteStatement&& other) noexcept {
    if (this != &other) {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
        stmt_ = std::exchange(other.stmt_, nullptr);
    }
    return *this;
}

core::Result<void, core::Error> SqliteStatement::bind_text(int idx,
                                                            std::string_view value) {
    int rc = sqlite3_bind_text(stmt_, idx, value.data(), value.size(), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string("SQLite bind_text failed: ") + sqlite3_errstr(rc)});
    }
    return {};
}

core::Result<void, core::Error> SqliteStatement::bind_int(int idx, int64_t value) {
    int rc = sqlite3_bind_int64(stmt_, idx, value);
    if (rc != SQLITE_OK) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string("SQLite bind_int failed: ") + sqlite3_errstr(rc)});
    }
    return {};
}

core::Result<void, core::Error> SqliteStatement::bind_null(int idx) {
    int rc = sqlite3_bind_null(stmt_, idx);
    if (rc != SQLITE_OK) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string("SQLite bind_null failed: ") + sqlite3_errstr(rc)});
    }
    return {};
}

bool SqliteStatement::step() {
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_ROW;
}

std::string SqliteStatement::column_text(int idx) const {
    const unsigned char* text = sqlite3_column_text(stmt_, idx);
    if (!text) return {};
    return std::string(reinterpret_cast<const char*>(text));
}

int64_t SqliteStatement::column_int(int idx) const {
    return sqlite3_column_int64(stmt_, idx);
}

bool SqliteStatement::column_is_null(int idx) const {
    return sqlite3_column_type(stmt_, idx) == SQLITE_NULL;
}

void SqliteStatement::reset() {
    sqlite3_reset(stmt_);
}

// ============================================================================
// SqliteDatabase
// ============================================================================

SqliteDatabase::SqliteDatabase(const std::filesystem::path& path) : path_(path) {}

SqliteDatabase::~SqliteDatabase() {
    close();
}

SqliteDatabase::SqliteDatabase(SqliteDatabase&& other) noexcept
    : path_(std::move(other.path_)), db_(std::exchange(other.db_, nullptr)) {}

SqliteDatabase& SqliteDatabase::operator=(SqliteDatabase&& other) noexcept {
    if (this != &other) {
        close();
        path_ = std::move(other.path_);
        db_   = std::exchange(other.db_, nullptr);
    }
    return *this;
}

core::Result<void, core::Error> SqliteDatabase::open() {
    if (db_) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database already open"});
    }

    int rc = sqlite3_open(path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string msg = "SQLite open failed: ";
        if (db_) {
            msg += sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
        } else {
            msg += sqlite3_errstr(rc);
        }
        return core::fail(core::Error{core::StatusCode::Internal, msg});
    }

    return {};
}

void SqliteDatabase::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SqliteDatabase::is_open() const noexcept {
    return db_ != nullptr;
}

core::Result<void, core::Error> SqliteDatabase::execute(std::string_view sql) {
    if (!db_) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    char* err_msg = nullptr;
    int rc        = sqlite3_exec(db_, sql.data(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string msg = "SQLite execute failed: ";
        if (err_msg) {
            msg += err_msg;
            sqlite3_free(err_msg);
        } else {
            msg += sqlite3_errstr(rc);
        }
        return core::fail(core::Error{core::StatusCode::Internal, msg});
    }

    return {};
}

core::Result<SqliteStatement, core::Error> SqliteDatabase::prepare(
    std::string_view sql) {
    if (!db_) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.data(), sql.size(), &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string("SQLite prepare failed: ") + sqlite3_errmsg(db_)});
    }

    return SqliteStatement{stmt};
}

core::Result<void, core::Error> SqliteDatabase::begin_transaction() {
    return execute("BEGIN TRANSACTION");
}

core::Result<void, core::Error> SqliteDatabase::commit() {
    return execute("COMMIT");
}

core::Result<void, core::Error> SqliteDatabase::rollback() {
    return execute("ROLLBACK");
}

int64_t SqliteDatabase::last_insert_rowid() const {
    if (!db_) return 0;
    return sqlite3_last_insert_rowid(db_);
}

} // namespace pvpgn::infra::persistence::sqlite
