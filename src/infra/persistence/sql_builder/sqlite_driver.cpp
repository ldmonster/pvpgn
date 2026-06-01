// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/sql_builder/sqlite_driver.hpp"

#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::persistence {

namespace {

/// Adapter to convert SQLite Row to DbRow interface
class SqliteDbRow final : public DbRow {
public:
    explicit SqliteDbRow(const pvpgn::infra::sqlite::Row& row) : row_(row) {}

    std::string get_text(int col) const override {
        return row_.get_text(col);
    }

    std::int64_t get_int(int col) const override {
        return row_.get_int(col);
    }

    std::span<const std::byte> get_blob(int col) const override {
        return row_.get_blob(col);
    }

    bool is_null(int col) const override {
        return row_.is_null(col);
    }

    int column_count() const override {
        return row_.column_count();
    }

private:
    const pvpgn::infra::sqlite::Row& row_;
};

}  // namespace

SqliteDriver::SqliteDriver(std::shared_ptr<pvpgn::infra::sqlite::SQLiteConnection> conn)
    : conn_(std::move(conn)), in_transaction_(false) {}

SqliteDriver::~SqliteDriver() = default;

core::Result<void, core::Error> SqliteDriver::exec(std::string_view sql) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }
    return conn_->exec(sql);
}

core::Result<void, core::Error> SqliteDriver::query(
    std::string_view sql,
    const DbRowCallback& cb) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    return conn_->query(sql, [&cb](const pvpgn::infra::sqlite::Row& row) {
        SqliteDbRow db_row(row);
        return cb(db_row);
    });
}

core::Result<void, core::Error> SqliteDriver::query_bind(
    std::string_view sql,
    std::initializer_list<DbParamValue> params,
    const DbRowCallback& cb) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    // Convert DbParamValue to SQLite ParamValue
    std::vector<pvpgn::infra::sqlite::SQLiteConnection::ParamValue> sqlite_params;
    for (const auto& param : params) {
        if (std::holds_alternative<std::int64_t>(param)) {
            sqlite_params.push_back(std::get<std::int64_t>(param));
        } else if (std::holds_alternative<std::string>(param)) {
            sqlite_params.push_back(std::get<std::string>(param));
        } else if (std::holds_alternative<std::vector<std::byte>>(param)) {
            sqlite_params.push_back(std::get<std::vector<std::byte>>(param));
        } else {
            sqlite_params.push_back(nullptr);
        }
    }

    return conn_->query_bind(sql, sqlite_params, [&cb](const pvpgn::infra::sqlite::Row& row) {
        SqliteDbRow db_row(row);
        return cb(db_row);
    });
}

std::int64_t SqliteDriver::last_insert_rowid() const {
    if (!conn_) {
        return 0;
    }
    return conn_->last_insert_rowid();
}

core::Result<void, core::Error> SqliteDriver::begin_transaction() {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }
    auto result = conn_->exec("BEGIN TRANSACTION");
    if (result.has_value()) {
        in_transaction_ = true;
    }
    return result;
}

core::Result<void, core::Error> SqliteDriver::commit() {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }
    auto result = conn_->exec("COMMIT");
    if (result.has_value()) {
        in_transaction_ = false;
    }
    return result;
}

core::Result<void, core::Error> SqliteDriver::rollback() {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }
    auto result = conn_->exec("ROLLBACK");
    if (result.has_value()) {
        in_transaction_ = false;
    }
    return result;
}

bool SqliteDriver::in_transaction() const {
    return in_transaction_;
}

}  // namespace pvpgn::infra::persistence
