// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file adapter_registry.hpp
/// Registry for persistence backend adapters. Provides static factory
/// methods to create IUnitOfWorkFactory instances for different backends.

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "application/ports/unit_of_work_factory.hpp"

namespace pvpgn::infra::persistence {

/// Enumeration of supported backend types
enum class BackendType {
    InMemory,      ///< Thread-safe in-memory store (tests, development)
    File,          ///< Legacy flat-file based storage
    SQLite,        ///< Embedded SQLite database
    MySQL,         ///< MySQL/MariaDB via native driver
    PostgreSQL,    ///< PostgreSQL via native driver
    ODBC           ///< Generic ODBC bridge (SQL Server, etc.)
};

/// Configuration for persistence backends
struct PersistenceConfig {
    BackendType backend{BackendType::InMemory};
    std::string connection_string;  ///< Connection URL for SQL backends
    std::string data_dir;           ///< Data directory for file-based backends
    std::uint32_t pool_size{4};     ///< Connection pool size for SQL
};

/// Registry for persistence backend factories.
/// Uses static registry pattern to support multiple backend implementations.
class AdapterRegistry {
public:
    /// Type alias for factory function
    using FactoryFunction = std::function<
        std::unique_ptr<application::ports::IUnitOfWorkFactory>(
            const PersistenceConfig&)>;

    /// Register a factory function for a given backend type.
    /// Call during application initialization before creating UnitOfWork.
    static void register_backend(BackendType type, FactoryFunction factory);

    /// Create a UnitOfWorkFactory for the configured backend.
    /// Returns an error if the backend is not registered.
    static std::unique_ptr<application::ports::IUnitOfWorkFactory> create(
        const PersistenceConfig& config);

    /// Get list of currently registered backend types.
    static std::vector<BackendType> available_backends();

    /// Clear all registered backends (useful for testing).
    static void clear();

private:
    // Static factory map: BackendType -> FactoryFunction
    static std::map<BackendType, FactoryFunction>& registry();
};

}  // namespace pvpgn::infra::persistence
