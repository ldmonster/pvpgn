// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/adapter_registry.hpp"

namespace pvpgn::infra::persistence {

void AdapterFactory::register_backend(BackendType type,
                                      FactoryFunction factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_[type] = std::move(factory);
}

std::unique_ptr<application::ports::IUnitOfWorkFactory>
AdapterFactory::create(const PersistenceConfig& config) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = registry_.find(config.backend);
    if (it == registry_.end()) {
        return nullptr;
    }
    return it->second(config);
}

std::vector<BackendType> AdapterFactory::available_backends() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BackendType> result;
    result.reserve(registry_.size());
    for (const auto& [type, _] : registry_) {
        result.push_back(type);
    }
    return result;
}

void AdapterFactory::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_.clear();
}

}  // namespace pvpgn::infra::persistence
