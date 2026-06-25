// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ignore_store.hpp
/// In-memory IIgnoreStore — per-account squelch lists. Thread-safe; used by the
/// inmemory bnetd backend and by tests.

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#include "application/chat/ignore_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryIgnoreStore final : public application::chat::IIgnoreStore {
public:
    bool squelch(domain::AccountId owner, domain::AccountId target) override {
        std::unique_lock lock(mutex_);
        return by_owner_[owner.value()].insert(target.value()).second;
    }

    bool unsquelch(domain::AccountId owner, domain::AccountId target) override {
        std::unique_lock lock(mutex_);
        auto it = by_owner_.find(owner.value());
        if (it == by_owner_.end()) return false;
        return it->second.erase(target.value()) > 0;
    }

    [[nodiscard]] bool
    ignores(domain::AccountId owner, domain::AccountId target) const override {
        std::shared_lock lock(mutex_);
        auto it = by_owner_.find(owner.value());
        if (it == by_owner_.end()) return false;
        return it->second.count(target.value()) > 0;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> by_owner_;
};

}  // namespace pvpgn::infra::inmemory
