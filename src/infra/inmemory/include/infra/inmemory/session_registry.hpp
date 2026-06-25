// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_registry.hpp
/// In-memory implementation of `ports::ISessionRegistry`. Maintains
/// the bidirectional `session ↔ account` mapping with O(1) lookup in
/// both directions.

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "domain/identity/ports.hpp"


namespace pvpgn::infra::inmemory {

class InMemorySessionRegistry final
    : public domain::identity::ISessionRegistry {
public:
    core::Status<> attach(domain::SessionId session,
                          domain::AccountId account) override {
        // Check-then-act (the one-account/one-session invariant) is performed
        // atomically: the guard checks and both inserts happen under a single
        // exclusive lock that is never released between them.
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (account_for_session_.contains(session.value())) {
            return core::fail(core::Error{
                core::StatusCode::AlreadyExists,
                "session already attached"});
        }
        if (session_for_account_.contains(account.value())) {
            return core::fail(core::Error{
                core::StatusCode::AlreadyExists,
                "account already has a session"});
        }
        account_for_session_[session.value()] = account.value();
        session_for_account_[account.value()] = session.value();
        return core::ok();
    }

    void detach(domain::SessionId session) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = account_for_session_.find(session.value());
        if (it == account_for_session_.end()) return;
        session_for_account_.erase(it->second);
        account_for_session_.erase(it);
    }

    std::optional<domain::SessionId>
    session_for(domain::AccountId account) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = session_for_account_.find(account.value());
        if (it == session_for_account_.end()) return std::nullopt;
        return domain::SessionId{it->second};
    }

    std::optional<domain::AccountId>
    account_for(domain::SessionId session) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = account_for_session_.find(session.value());
        if (it == account_for_session_.end()) return std::nullopt;
        return domain::AccountId{it->second};
    }

    std::vector<domain::SessionId> list() const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<domain::SessionId> out;
        out.reserve(account_for_session_.size());
        for (const auto& [s, _a] : account_for_session_) {
            out.emplace_back(s);
        }
        std::sort(out.begin(), out.end(),
                  [](auto a, auto b) { return a.value() < b.value(); });
        return out;
    }

private:
    // Guards both maps. `shared_lock` for read-only accessors (session_for,
    // account_for, list), `unique_lock` for mutators (attach, detach). All
    // accessors return by value (std::optional / std::vector), so no
    // reference/pointer/iterator into the maps escapes the critical section.
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint64_t, std::uint32_t> account_for_session_;
    std::unordered_map<std::uint32_t, std::uint64_t> session_for_account_;
};

}  // namespace pvpgn::infra::inmemory
