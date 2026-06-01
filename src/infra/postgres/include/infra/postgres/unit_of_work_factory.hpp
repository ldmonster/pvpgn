// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// PostgreSQL-backed Unit of Work factory.

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "application/persistence/unit_of_work_factory.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

namespace pvpgn::infra::postgres {

/// PostgreSQL factory for creating IUnitOfWork instances.
class PostgreSQLUnitOfWorkFactory final : public application::ports::IUnitOfWorkFactory {
public:
    /// Create a factory from a connection string.
    /// @param connection_string "host:port/database" format
    explicit PostgreSQLUnitOfWorkFactory(std::string_view connection_string);

    ~PostgreSQLUnitOfWorkFactory() = default;

    /// Create a new unit of work (connection).
    std::unique_ptr<application::ports::IUnitOfWork> create() override;

private:
    std::string connection_string_;
    std::string host_;
    std::uint16_t port_;
    std::string database_;
    std::string user_;
    std::string password_;

    /// Parse connection string and extract components.
    void parse_connection_string();
};

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
