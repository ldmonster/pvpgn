// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/result.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace pvpgn::infra::persistence::sqlite {

/// RAII wrapper for sqlite3_stmt with bind/step/column operations.
class SqliteStatement {
public:
    explicit SqliteStatement(sqlite3_stmt* stmt);
    ~SqliteStatement();

    // Non-copyable, movable
    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;
    SqliteStatement(SqliteStatement&& other) noexcept;
    SqliteStatement& operator=(SqliteStatement&& other) noexcept;

    // Bind parameters
    core::Result<void, core::Error> bind_text(int idx, std::string_view value);
    core::Result<void, core::Error> bind_int(int idx, int64_t value);
    core::Result<void, core::Error> bind_null(int idx);

    // Step and read results
    bool step();  // returns true if row available

    std::string column_text(int idx) const;
    int64_t column_int(int idx) const;
    bool column_is_null(int idx) const;

    void reset();

private:
    sqlite3_stmt* stmt_;
};

/// RAII wrapper for sqlite3 database connection.
class SqliteDatabase {
public:
    explicit SqliteDatabase(const std::filesystem::path& path);
    ~SqliteDatabase();

    // Non-copyable, movable
    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&& other) noexcept;
    SqliteDatabase& operator=(SqliteDatabase&& other) noexcept;

    core::Result<void, core::Error> open();
    void close();
    bool is_open() const noexcept;

    core::Result<void, core::Error> execute(std::string_view sql);
    core::Result<SqliteStatement, core::Error> prepare(std::string_view sql);

    // Transaction helpers
    core::Result<void, core::Error> begin_transaction();
    core::Result<void, core::Error> commit();
    core::Result<void, core::Error> rollback();

    int64_t last_insert_rowid() const;

private:
    std::filesystem::path path_;
    sqlite3* db_ = nullptr;
};

} // namespace pvpgn::infra::persistence::sqlite
