// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_user_flags_store.hpp
/// In-memory IWolUserFlagsStore — AccountId -> WOL find/page flags. Thread-safe;
/// used by the inmemory bnetd backend and tests. Mirrors the other inmemory
/// session-scoped WOL stores.

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "application/game/wol_user_flags_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryWolUserFlagsStore final
    : public application::game::IWolUserFlagsStore {
public:
    void set(domain::AccountId account,
             application::game::WolUserFlags flags) override {
        std::unique_lock lock(mutex_);
        by_account_[account.value()] = flags;
    }

    [[nodiscard]] application::game::WolUserFlags
    get(domain::AccountId account) const override {
        std::shared_lock lock(mutex_);
        auto it = by_account_.find(account.value());
        if (it == by_account_.end()) return {};  // defaults: both ON
        return it->second;
    }

    void remove(domain::AccountId account) override {
        std::unique_lock lock(mutex_);
        by_account_.erase(account.value());
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint64_t, application::game::WolUserFlags>
        by_account_;
};

}  // namespace pvpgn::infra::inmemory
