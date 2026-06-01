// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file db_driver.hpp
/// Database driver interface for multi-backend support.

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::persistence {

/// Represents a single row returned from a query.
class DbRow {
public:
    virtual ~DbRow() = default;

    virtual std::string get_text(int col) const = 0;
    virtual std::int64_t get_int(int col) const = 0;
    virtual std::span<const std::byte> get_blob(int col) const = 0;
    virtual bool is_null(int col) const = 0;
    virtual int column_count() const = 0;
};

using DbRowCallback = std::function<bool(const DbRow&)>;
using DbParamValue = std::variant<std::int64_t, std::string, std::vector<std::byte>, std::nullptr_t>;

/// Database driver interface.
class IDbDriver {
public:
    virtual ~IDbDriver() = default;

    IDbDriver(const IDbDriver&) = delete;
    IDbDriver& operator=(const IDbDriver&) = delete;
    IDbDriver(IDbDriver&&) = delete;
    IDbDriver& operator=(IDbDriver&&) = delete;

    virtual core::Result<void, core::Error> exec(std::string_view sql) = 0;

    virtual core::Result<void, core::Error> query(
        std::string_view sql,
        const DbRowCallback& cb) = 0;

    virtual core::Result<void, core::Error> query_bind(
        std::string_view sql,
        std::initializer_list<DbParamValue> params,
        const DbRowCallback& cb) = 0;

    virtual std::int64_t last_insert_rowid() const = 0;

    virtual core::Result<void, core::Error> begin_transaction() = 0;
    virtual core::Result<void, core::Error> commit() = 0;
    virtual core::Result<void, core::Error> rollback() = 0;
    virtual bool in_transaction() const = 0;

protected:
    IDbDriver() = default;
};

}  // namespace pvpgn::infra::persistence
