// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Thread-safe in-memory store. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "application/ports/account_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryAccountRepository final
    : public application::ports::IAccountRepository {
public:
    core::Result<domain::identity::Account, core::Error>
    find_by_id(uint32_t id) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: id not found"});
        }
        return *it->second;
    }

    core::Result<domain::identity::Account, core::Error>
    find_by_name(std::string_view name) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        // Normalize name to canonical form for lookup
        std::string canonical{name};
        std::transform(canonical.begin(), canonical.end(), canonical.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        auto it = by_canonical_.find(canonical);
        if (it == by_canonical_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: name not found"});
        }
        auto a = by_id_.find(it->second);
        if (a == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::Internal, "account: index corrupt"});
        }
        return *a->second;
    }

    core::Result<void, core::Error>
    save(const domain::identity::Account& account) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_unique<domain::identity::Account>(account);
        std::string canonical{account.name().canonical()};
        by_canonical_[canonical] = account.id().value();
        by_id_[account.id().value()] = std::move(copy);
        return core::ok();
    }

    core::Result<void, core::Error>
    remove(std::string_view name) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        std::string canonical{name};
        std::transform(canonical.begin(), canonical.end(), canonical.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        auto it = by_canonical_.find(canonical);
        if (it == by_canonical_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: name not found"});
        }
        uint32_t id = it->second;
        by_canonical_.erase(it);
        by_id_.erase(id);
        return core::ok();
    }

    core::Result<bool, core::Error>
    exists(std::string_view name) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::string canonical{name};
        std::transform(canonical.begin(), canonical.end(), canonical.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return by_canonical_.find(canonical) != by_canonical_.end();
    }

    core::Result<std::vector<domain::identity::Account>, core::Error>
    list_online() override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<domain::identity::Account> result;
        for (const auto& [id, account] : by_id_) {
            result.push_back(*account);
        }
        return result;
    }

    core::Result<uint32_t, core::Error>
    count() override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return static_cast<uint32_t>(by_id_.size());
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<domain::identity::Account>> by_id_;
    std::unordered_map<std::string, std::uint32_t> by_canonical_;
};

}  // namespace pvpgn::infra::inmemory
