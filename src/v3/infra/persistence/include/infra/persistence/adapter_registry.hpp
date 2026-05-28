// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file adapter_registry.hpp
/// Instance-based factory for persistence backend adapters.
/// Replaces the former static AdapterRegistry with an injectable AdapterFactory.

#include <functional>
#include <map>
#include <memory>
#include <mutex>
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

/// Instance-based factory for persistence backend adapters.
/// Holds a per-instance registry of backend factory functions.
/// Inject this as a dependency rather than relying on global/static state.
class AdapterFactory {
public:
    /// Type alias for factory function
    using FactoryFunction = std::function<
        std::unique_ptr<application::ports::IUnitOfWorkFactory>(
            const PersistenceConfig&)>;

    AdapterFactory() = default;
    ~AdapterFactory() = default;

    // Non-copyable, movable
    AdapterFactory(const AdapterFactory&) = delete;
    AdapterFactory& operator=(const AdapterFactory&) = delete;
    AdapterFactory(AdapterFactory&&) = default;
    AdapterFactory& operator=(AdapterFactory&&) = default;

    /// Register a factory function for a given backend type.
    void register_backend(BackendType type, FactoryFunction factory);

    /// Create a UnitOfWorkFactory for the configured backend.
    /// Returns nullptr if the backend is not registered.
    std::unique_ptr<application::ports::IUnitOfWorkFactory> create(
        const PersistenceConfig& config) const;

    /// Get list of currently registered backend types.
    std::vector<BackendType> available_backends() const;

    /// Clear all registered backends (useful for testing).
    void clear();

private:
    mutable std::mutex mutex_;
    std::map<BackendType, FactoryFunction> registry_;
};

}  // namespace pvpgn::infra::persistence
