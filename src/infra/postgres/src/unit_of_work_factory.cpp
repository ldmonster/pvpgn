// SPDX-License-Identifier: GPL-2.0-or-later

/// @file unit_of_work_factory.cpp
/// PostgreSQL-backed IUnitOfWorkFactory implementation.
///
/// Compiled only when PVPGN_V3_WITH_POSTGRESQL is defined.
/// Parses a connection string of the form "host:port:user:pass:db"
/// and creates PostgreSQLUnitOfWork instances.

#include "infra/postgres/unit_of_work_factory.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

#include <stdexcept>
#include <string>
#include <vector>

#include "infra/postgres/unit_of_work.hpp"

#include "application/persistence/unit_of_work.hpp"

namespace pvpgn::infra::postgres {

namespace {

/// Split a string by a delimiter into at most max_parts parts.
std::vector<std::string> split(std::string_view s, char delim, std::size_t max_parts = 0) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= s.size()) {
        if (max_parts > 0 && parts.size() + 1 == max_parts) {
            parts.emplace_back(s.substr(start));
            break;
        }
        const auto pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(s.substr(start));
            break;
        }
        parts.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

}  // namespace

PostgreSQLUnitOfWorkFactory::PostgreSQLUnitOfWorkFactory(std::string_view connection_string)
    : connection_string_(connection_string)
    , host_("localhost")
    , port_(5432)
    , database_("pvpgn")
    , user_("pvpgn")
    , password_("") {
    parse_connection_string();
}

void PostgreSQLUnitOfWorkFactory::parse_connection_string() {
    // Expected format: "host:port:user:pass:db"
    // e.g. "127.0.0.1:5432:pvpgn:secret:pvpgn_db"
    const auto parts = split(connection_string_, ':', 5);
    if (parts.size() >= 1 && !parts[0].empty()) host_     = parts[0];
    if (parts.size() >= 2 && !parts[1].empty()) {
        try { port_ = static_cast<std::uint16_t>(std::stoul(parts[1])); }
        catch (...) {}
    }
    if (parts.size() >= 3) user_     = parts[2];
    if (parts.size() >= 4) password_ = parts[3];
    if (parts.size() >= 5) database_ = parts[4];
}

std::unique_ptr<application::ports::IUnitOfWork>
PostgreSQLUnitOfWorkFactory::create() {
    auto conn = std::make_shared<PostgreSQLConnection>(
        host_, port_, user_, password_, database_);
    return std::make_unique<PostgreSQLUnitOfWork>(std::move(conn));
}

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
