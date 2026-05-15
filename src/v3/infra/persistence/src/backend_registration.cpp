// SPDX-License-Identifier: GPL-2.0-or-later

/// @file backend_registration.cpp
/// Backend registration initialization.
/// Registers SQLite and File backends with AdapterRegistry.

#include "infra/persistence/adapter_registry.hpp"

#include "infra/sqlite/unit_of_work_factory.hpp"
#include "infra/file/unit_of_work_factory.hpp"

namespace pvpgn::infra::persistence {

/// Initialize all persistence backends.
/// Call this during application startup before creating unit of work.
void register_backends() {
    // Register SQLite backend
    AdapterRegistry::register_backend(
        BackendType::SQLite,
        [](const PersistenceConfig& config) {
            return std::make_unique<sqlite::SQLiteUnitOfWorkFactory>(
                config.connection_string);
        });

    // Register File backend
    AdapterRegistry::register_backend(
        BackendType::File,
        [](const PersistenceConfig& config) {
            return std::make_unique<file::FileUnitOfWorkFactory>(
                config.data_dir, config.data_dir + "/bnban.conf");
        });
}

}  // namespace pvpgn::infra::persistence
