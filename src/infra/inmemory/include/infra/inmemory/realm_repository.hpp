// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_repository.hpp
/// Thread-safe in-memory implementation of IRealmRepository.
/// Suitable for tests and development.

#include <ankerl/unordered_dense.h>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>

#include "domain/realm/ports.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryRealmRepository final
    : public application::ports::IRealmRepository {
public:
    core::Result<domain::realm::Realm>
    find_by_id(std::uint32_t id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(
                core::Error{core::StatusCode::NotFound, "realm: id not found"});
        }
        return *it->second;
    }

    core::Result<domain::realm::Realm>
    find_by_name(const std::string& name) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        // Normalize to lowercase for case-insensitive lookup
        std::string normalized_name(name);
        std::transform(normalized_name.begin(), normalized_name.end(),
                      normalized_name.begin(), ::tolower);
        auto it = by_name_.find(normalized_name);
        if (it == by_name_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "realm: name not found"});
        }
        auto realm_it = by_id_.find(it->second);
        if (realm_it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::Internal, "realm: index corrupt"});
        }
        return *realm_it->second;
    }

    core::Status<>
    save(const domain::realm::Realm& realm) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_unique<domain::realm::Realm>(realm);
        
        // Normalize name for index
        std::string normalized_name(realm.name());
        std::transform(normalized_name.begin(), normalized_name.end(),
                      normalized_name.begin(), ::tolower);
        by_name_[normalized_name] = realm.id();
        
        by_id_[realm.id()] = std::move(copy);
        return core::ok();
    }

    core::Status<>
    remove(std::uint32_t id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(
                core::Error{core::StatusCode::NotFound, "realm: id not found"});
        }
        
        // Remove from name index
        std::string normalized_name(it->second->name());
        std::transform(normalized_name.begin(), normalized_name.end(),
                      normalized_name.begin(), ::tolower);
        by_name_.erase(normalized_name);
        
        by_id_.erase(it);
        return core::ok();
    }

    void forEach(
        std::function<bool(const domain::realm::Realm&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [_id, realm] : by_id_) {
            if (!predicate(*realm)) break;
        }
    }

    std::size_t size() const noexcept override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return by_id_.size();
    }

private:
    mutable std::shared_mutex mutex_;
    ankerl::unordered_dense::map<std::uint32_t,
                                 std::unique_ptr<domain::realm::Realm>>
        by_id_;
    ankerl::unordered_dense::map<std::string, std::uint32_t> by_name_;
};

}  // namespace pvpgn::infra::inmemory
