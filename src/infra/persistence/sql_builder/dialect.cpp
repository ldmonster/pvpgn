// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/sql_builder/dialect.hpp"

#include <sstream>

namespace pvpgn::infra::persistence {

std::string_view SqlDialectHelper::placeholder() const {
    switch (dialect_) {
        case SqlDialect::SQLite:
        case SqlDialect::MySQL:
            return "?";
        case SqlDialect::Postgres:
            return "$1";  // Caller must track position
    }
    return "?";
}

std::string SqlDialectHelper::placeholder_at(int position) const {
    switch (dialect_) {
        case SqlDialect::SQLite:
        case SqlDialect::MySQL:
            return "?";
        case SqlDialect::Postgres: {
            std::ostringstream oss;
            oss << "$" << position;
            return oss.str();
        }
    }
    return "?";
}

std::string_view SqlDialectHelper::upsert_prefix() const {
    switch (dialect_) {
        case SqlDialect::SQLite:
            return "INSERT OR REPLACE";
        case SqlDialect::MySQL:
            return "INSERT";
        case SqlDialect::Postgres:
            return "INSERT";
    }
    return "INSERT";
}

char SqlDialectHelper::quote_char() const {
    switch (dialect_) {
        case SqlDialect::SQLite:
        case SqlDialect::Postgres:
            return '"';
        case SqlDialect::MySQL:
            return '`';
    }
    return '"';
}

std::string SqlDialectHelper::quote_identifier(std::string_view name) const {
    char quote = quote_char();
    std::string result;
    result.push_back(quote);
    result.append(name);
    result.push_back(quote);
    return result;
}

std::string_view SqlDialectHelper::returning_prefix() const {
    switch (dialect_) {
        case SqlDialect::SQLite:
        case SqlDialect::MySQL:
            return "";
        case SqlDialect::Postgres:
            return "RETURNING";
    }
    return "";
}

bool SqlDialectHelper::supports_returning() const {
    return dialect_ == SqlDialect::Postgres;
}

}  // namespace pvpgn::infra::persistence
