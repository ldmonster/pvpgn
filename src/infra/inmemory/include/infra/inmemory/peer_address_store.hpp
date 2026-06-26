// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file peer_address_store.hpp
/// In-memory IPeerAddressStore — AccountId -> peer IP string. Thread-safe;
/// used by the inmemory bnetd backend and tests. Mirrors the other inmemory
/// session-scoped stores.

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "domain/connection/peer_address_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryPeerAddressStore final
    : public domain::connection::IPeerAddressStore {
public:
    void set(domain::AccountId account, std::string address) override {
        std::unique_lock lock(mutex_);
        by_account_[account.value()] = std::move(address);
    }

    [[nodiscard]] std::optional<std::string>
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
    std::unordered_map<std::uint64_t, std::string> by_account_;
};

}  // namespace pvpgn::infra::inmemory
