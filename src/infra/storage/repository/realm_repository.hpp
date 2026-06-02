// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_repository.hpp
/// Thread-safe in-memory store for realms. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <cctype>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "domain/realm/ports.hpp"

namespace pvpgn::infra::storage {

class InMemoryRealmRepository final
    : public domain::realm::IRealmRepository {
public:
    core::Result<domain::realm::Realm>
    find_by_id(std::uint32_t id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "realm: id not found"});
        }
        return *it->second;
    }

    core::Result<domain::realm::Realm>
    find_by_name(const std::string& name) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::string canonical = canonicalize_(name);
        auto it = by_name_.find(canonical);
        if (it == by_name_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "realm: name not found"});
        }
        auto a = by_id_.find(it->second);
        if (a == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::Internal, "realm: index corrupt"});
        }
        return *a->second;
    }

    core::Status<>
    save(const domain::realm::Realm& realm) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_unique<domain::realm::Realm>(realm);
        std::string canonical = canonicalize_(realm.name());
        by_name_[canonical] = realm.id();
        by_id_[realm.id()] = std::move(copy);
        return core::ok();
    }

    core::Status<> remove(std::uint32_t id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "realm: id not found"});
        }
        std::string canonical = canonicalize_(it->second->name());
        by_name_.erase(canonical);
        by_id_.erase(it);
        return core::ok();
    }

    void forEach(std::function<bool(const domain::realm::Realm&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [id, realm] : by_id_) {
            if (!predicate(*realm)) break;
        }
    }

    std::size_t size() const noexcept override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return by_id_.size();
    }

private:
    static std::string canonicalize_(const std::string& s) noexcept {
        std::string result;
        result.reserve(s.size());
        for (unsigned char c : s) {
            result.push_back(std::tolower(c));
        }
        return result;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<domain::realm::Realm>> by_id_;
    std::unordered_map<std::string, std::uint32_t> by_name_;
};

}  // namespace pvpgn::infra::storage
