// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/adapter_registry.hpp"

#include <mutex>

namespace pvpgn::infra::persistence {

static std::mutex& registry_mutex() {
    static std::mutex m;
    return m;
}

std::map<BackendType, AdapterRegistry::FactoryFunction>&
AdapterRegistry::registry() {
    static std::map<BackendType, FactoryFunction> r;
    return r;
}

void AdapterRegistry::register_backend(BackendType type,
                                       FactoryFunction factory) {
    std::lock_guard<std::mutex> lock(registry_mutex());
    registry()[type] = factory;
}

std::unique_ptr<application::ports::IUnitOfWorkFactory> AdapterRegistry::create(
    const PersistenceConfig& config) {
    std::lock_guard<std::mutex> lock(registry_mutex());
    auto& r = registry();
    auto it = r.find(config.backend);
    if (it == r.end()) {
        return nullptr;
    }
    return it->second(config);
}

std::vector<BackendType> AdapterRegistry::available_backends() {
    std::lock_guard<std::mutex> lock(registry_mutex());
    std::vector<BackendType> result;
    for (const auto& [type, _] : registry()) {
        result.push_back(type);
    }
    return result;
}

void AdapterRegistry::clear() {
    std::lock_guard<std::mutex> lock(registry_mutex());
    registry().clear();
}

}  // namespace pvpgn::infra::persistence
