// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_presence_store.hpp
/// In-memory IAccountPresenceStore — AccountId -> {clienttag, away, dnd}.
/// Thread-safe; used by the inmemory bnetd backend and tests. Mirrors the other
/// inmemory connection-scoped stores (peer_address_store).

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "domain/connection/account_presence_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryAccountPresenceStore final
    : public domain::connection::IAccountPresenceStore {
public:
    void set_client_tag(domain::AccountId account,
                        std::uint32_t client_tag) override {
        std::unique_lock lock(mutex_);
        // A fresh login resets away/DND to available (matches a new connection).
        auto& p = by_account_[account.value()];
        p.client_tag = client_tag;
        p.away = false;
        p.dnd = false;
    }

    void set_away(domain::AccountId account, bool away) override {
        std::unique_lock lock(mutex_);
        by_account_[account.value()].away = away;
    }

    void set_dnd(domain::AccountId account, bool dnd) override {
        std::unique_lock lock(mutex_);
        by_account_[account.value()].dnd = dnd;
    }

    [[nodiscard]] std::optional<domain::connection::AccountPresence>
    get(domain::AccountId account) const override {
        std::shared_lock lock(mutex_);
        auto it = by_account_.find(account.value());
        if (it == by_account_.end()) return std::nullopt;
        return it->second;
    }

    void remove(domain::AccountId account) override {
        std::unique_lock lock(mutex_);
        by_account_.erase(account.value());
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint64_t,
                       domain::connection::AccountPresence> by_account_;
};

}  // namespace pvpgn::infra::inmemory
