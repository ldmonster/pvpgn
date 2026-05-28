// SPDX-License-Identifier: GPL-2.0-or-later

/// @file backend_registration.cpp
/// Backend registration initialization.
/// Registers SQLite and File backends with an AdapterFactory instance.

#include "infra/persistence/adapter_registry.hpp"

#include "infra/sqlite/unit_of_work_factory.hpp"
#include "infra/file/unit_of_work_factory.hpp"

namespace pvpgn::infra::persistence {

/// Initialize all persistence backends into the given factory.
/// Call this during application startup before creating unit of work.
void register_backends(AdapterFactory& factory) {
    // Register SQLite backend
    factory.register_backend(
        BackendType::SQLite,
        [](const PersistenceConfig& config) {
            return std::make_unique<sqlite::SQLiteUnitOfWorkFactory>(
                config.connection_string);
        });

    // Register File backend
    factory.register_backend(
        BackendType::File,
        [](const PersistenceConfig& config) {
            return std::make_unique<file::FileUnitOfWorkFactory>(
                config.data_dir, config.data_dir + "/bnban.conf");
        });
}

}  // namespace pvpgn::infra::persistence
