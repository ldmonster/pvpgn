// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_permission_checker.hpp
/// In-memory fake for IPermissionChecker.
/// Suitable for tests and development/CI composition roots.

#include <cstdint>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "domain/moderation/ports.hpp"
#include "domain/shared/ids.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryPermissionChecker final
    : public domain::moderation::IPermissionChecker {
public:
    /// Grant a specific permission to an account (test helper).
    void grant(domain::AccountId account,
               domain::moderation::Permission perm) {
        std::unique_lock lock(mutex_);
        perms_[account.value()].insert(static_cast<std::uint16_t>(perm));
    }

    /// Revoke a specific permission from an account (test helper).
    void revoke(domain::AccountId account,
                domain::moderation::Permission perm) {
        std::unique_lock lock(mutex_);
        auto it = perms_.find(account.value());
        if (it != perms_.end()) {
            it->second.erase(static_cast<std::uint16_t>(perm));
        }
    }

    /// Grant membership in a command group to an account (test helper).
    void grant_group(domain::AccountId account, std::string group) {
        std::unique_lock lock(mutex_);
        groups_[account.value()].insert(std::move(group));
    }

    /// Revoke membership in a command group from an account (test helper).
    void revoke_group(domain::AccountId account, std::string_view group) {
        std::unique_lock lock(mutex_);
        auto it = groups_.find(account.value());
        if (it != groups_.end()) {
            it->second.erase(std::string{group});
        }
    }

    [[nodiscard]] bool
    has_permission(domain::AccountId account,
                   domain::moderation::Permission perm) const override {
        std::shared_lock lock(mutex_);
        auto it = perms_.find(account.value());
        if (it == perms_.end()) return false;
        return it->second.count(static_cast<std::uint16_t>(perm)) > 0;
    }

    [[nodiscard]] bool
    has_command_group(domain::AccountId account,
                      std::string_view group) const override {
        std::shared_lock lock(mutex_);
        auto it = groups_.find(account.value());
        if (it == groups_.end()) return false;
        return it->second.count(std::string{group}) > 0;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint32_t,
                       std::unordered_set<std::uint16_t>> perms_;
    std::unordered_map<std::uint32_t,
                       std::set<std::string>> groups_;
};

}  // namespace pvpgn::infra::inmemory
