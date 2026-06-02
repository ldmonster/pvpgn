// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file recording_fake_driver.hpp
/// Shared test double for the consolidated persistence repositories (Plan 07).
///
/// `RecordingFakeDriver` is an `IDbDriver` that records every statement
/// (SQL + bound params) and replays a programmable list of rows, so a
/// repository's SQL generation, parameter binding and row → domain mapping can
/// be unit-tested WITHOUT a live database. This runs in any environment; the
/// sqlite/mysql/postgres integration matrix stays separate (and is env-gated
/// where sqlite3.h is unavailable).

#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::test::persistence {

using pvpgn::infra::persistence::DbParamValue;
using pvpgn::infra::persistence::DbRow;
using pvpgn::infra::persistence::DbRowCallback;
using pvpgn::infra::persistence::IDbDriver;

/// A single cell of a `FakeRow`: integer, text, or NULL.
using Cell = std::variant<std::int64_t, std::string, std::nullptr_t>;

/// A programmable `DbRow` backed by an explicit list of cells.
class FakeRow final : public DbRow {
public:
    explicit FakeRow(std::vector<Cell> cells) : cells_(std::move(cells)) {}

    std::string get_text(int col) const override {
        return std::get<std::string>(cells_.at(static_cast<std::size_t>(col)));
    }
    std::int64_t get_int(int col) const override {
        return std::get<std::int64_t>(cells_.at(static_cast<std::size_t>(col)));
    }
    std::span<const std::byte> get_blob(int) const override { return {}; }
    bool is_null(int col) const override {
        return std::holds_alternative<std::nullptr_t>(
            cells_.at(static_cast<std::size_t>(col)));
    }
    int column_count() const override {
        return static_cast<int>(cells_.size());
    }

private:
    std::vector<Cell> cells_;
};

/// Records statements and replays programmable rows.
class RecordingFakeDriver final : public IDbDriver {
public:
    struct Call {
        std::string               sql;
        std::vector<DbParamValue> params;
    };

    std::vector<Call>    calls;
    std::vector<FakeRow> next_rows;  // replayed on EVERY query (single result set)

    // Optional: a queue of result sets, one consumed per query/query_bind (for
    // repositories that issue several queries per call, e.g. a parent row then
    // its child rows). When non-empty it takes precedence over `next_rows`.
    std::deque<std::vector<FakeRow>> result_sets;

    /// Enqueue a result set to be returned by the next query.
    void push_result_set(std::vector<FakeRow> rows) {
        result_sets.push_back(std::move(rows));
    }

    // Transaction bookkeeping (begin/commit/rollback are not recorded in
    // `calls` since they carry no SQL; tests assert on these counters).
    int begin_count{0};
    int commit_count{0};
    int rollback_count{0};

    pvpgn::core::Result<void, pvpgn::core::Error> exec(
        std::string_view sql) override {
        calls.push_back({std::string(sql), {}});
        return pvpgn::core::ok();
    }

    pvpgn::core::Result<void, pvpgn::core::Error> query(
        std::string_view sql, const DbRowCallback& cb) override {
        calls.push_back({std::string(sql), {}});
        replay(cb);
        return pvpgn::core::ok();
    }

    pvpgn::core::Result<void, pvpgn::core::Error> query_bind(
        std::string_view sql, std::initializer_list<DbParamValue> params,
        const DbRowCallback& cb) override {
        calls.push_back({std::string(sql), std::vector<DbParamValue>(params)});
        replay(cb);
        return pvpgn::core::ok();
    }

    std::int64_t last_insert_rowid() const override { return 0; }
    pvpgn::core::Result<void, pvpgn::core::Error> begin_transaction() override {
        ++begin_count;
        return pvpgn::core::ok();
    }
    pvpgn::core::Result<void, pvpgn::core::Error> commit() override {
        ++commit_count;
        return pvpgn::core::ok();
    }
    pvpgn::core::Result<void, pvpgn::core::Error> rollback() override {
        ++rollback_count;
        return pvpgn::core::ok();
    }
    bool in_transaction() const override {
        return begin_count > commit_count + rollback_count;
    }

    [[nodiscard]] const Call& last() const { return calls.back(); }

private:
    void replay(const DbRowCallback& cb) {
        if (!result_sets.empty()) {
            const auto rows = std::move(result_sets.front());
            result_sets.pop_front();
            for (const auto& row : rows) {
                if (!cb(row)) break;
            }
            return;
        }
        for (const auto& row : next_rows) {
            if (!cb(row)) break;
        }
    }
};

// Common param accessors for assertions.
inline std::int64_t as_int(const DbParamValue& p) {
    return std::get<std::int64_t>(p);
}
inline std::string as_str(const DbParamValue& p) {
    return std::get<std::string>(p);
}
inline bool is_null_param(const DbParamValue& p) {
    return std::holds_alternative<std::nullptr_t>(p);
}

}  // namespace pvpgn::test::persistence
