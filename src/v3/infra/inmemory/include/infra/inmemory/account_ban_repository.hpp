// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_ban_repository.hpp
/// Thread-safe in-memory implementation of IAccountBanRepository.
/// Suitable for tests and development.

#include <ankerl/unordered_dense.h>
#include <functional>
#include <shared_mutex>

#include "application/ports/account_ban_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryAccountBanRepository final
    : public application::ports::IAccountBanRepository {
public:
    core::Result<std::optional<application::ports::AccountBan>>
    find_active_ban(domain::AccountId account_id,
                    core::SystemTime now) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_account_.find(account_id.value());
        if (it == by_account_.end()) {
            return std::optional<application::ports::AccountBan>{};
        }
        
        const auto& ban = it->second;
        if (ban.active_at(now)) {
            return ban;
        }
        return std::optional<application::ports::AccountBan>{};
    }

    core::Status<>
    add_ban(const application::ports::AccountBan& ban) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        by_account_[ban.banned_account.value()] = ban;
        return core::ok();
    }

    core::Status<>
    remove_ban(domain::AccountId account_id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = by_account_.find(account_id.value());
        if (it == by_account_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound,
                "account_ban: not found"});
        }
        by_account_.erase(it);
        return core::ok();
    }

    void for_each(
        std::function<bool(const application::ports::AccountBan&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [_account_id, ban] : by_account_) {
            if (!predicate(ban)) break;
        }
    }

private:
    mutable std::shared_mutex mutex_;
    ankerl::unordered_dense::map<std::uint32_t,
                                 application::ports::AccountBan>
        by_account_;
};

}  // namespace pvpgn::infra::inmemory
