// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Thread-safe in-memory store. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "domain/identity/ports.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryAccountRepository final
    : public domain::identity::IAccountRepository {
public:
    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: id not found"});
        }
        return *it->second;
    }

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        const std::string canonical{name.canonical()};
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

    core::Status<>
    save(const domain::identity::Account& account) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_unique<domain::identity::Account>(account);
        const std::string canonical{account.name().canonical()};
        by_canonical_[canonical] = account.id().value();
        by_id_[account.id().value()] = std::move(copy);
        return core::ok();
    }

    core::Status<>
    remove(domain::AccountId id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: id not found"});
        }
        const std::string canonical{it->second->name().canonical()};
        by_id_.erase(it);
        by_canonical_.erase(canonical);
        return core::ok();
    }

    void forEach(
        std::function<bool(const domain::identity::Account&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [id, account] : by_id_) {
            if (!predicate(*account)) break;
        }
    }

    std::size_t size() const noexcept override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return by_id_.size();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<domain::identity::Account>> by_id_;
    std::unordered_map<std::string, std::uint32_t> by_canonical_;
};

}  // namespace pvpgn::infra::inmemory
