// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_registry.hpp
/// In-memory implementation of `ports::ISessionRegistry`. Maintains
/// the bidirectional `session ↔ account` mapping with O(1) lookup in
/// both directions.

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "application/ports/session_registry.hpp"

namespace pvpgn::infra::inmemory {

class InMemorySessionRegistry final
    : public application::ports::ISessionRegistry {
public:
    core::Status<> attach(domain::SessionId session,
                          domain::AccountId account) override {
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
        auto it = account_for_session_.find(session.value());
        if (it == account_for_session_.end()) return;
        session_for_account_.erase(it->second);
        account_for_session_.erase(it);
    }

    std::optional<domain::SessionId>
    session_for(domain::AccountId account) const override {
        auto it = session_for_account_.find(account.value());
        if (it == session_for_account_.end()) return std::nullopt;
        return domain::SessionId{it->second};
    }

    std::optional<domain::AccountId>
    account_for(domain::SessionId session) const override {
        auto it = account_for_session_.find(session.value());
        if (it == account_for_session_.end()) return std::nullopt;
        return domain::AccountId{it->second};
    }

    std::vector<domain::SessionId> list() const override {
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
    std::unordered_map<std::uint64_t, std::uint32_t> account_for_session_;
    std::unordered_map<std::uint32_t, std::uint64_t> session_for_account_;
};

}  // namespace pvpgn::infra::inmemory
