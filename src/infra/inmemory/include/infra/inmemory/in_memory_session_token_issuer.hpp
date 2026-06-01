// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_session_token_issuer.hpp
/// In-memory fake for ISessionTokenIssuer using an atomic counter.
/// Suitable for tests and development/CI composition roots.

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "domain/identity/ports.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::infra::inmemory {

class InMemorySessionTokenIssuer final
    : public application::ports::ISessionTokenIssuer {
public:
    std::string
    issue(domain::AccountId account_id) override {
        const std::uint64_t seq = counter_.fetch_add(1, std::memory_order_relaxed);
        std::string token = "tok-" + std::to_string(seq);
        std::unique_lock lock(mutex_);
        tokens_[token] = account_id;
        return token;
    }

    core::Result<domain::AccountId>
    validate(std::string_view token) override {
        std::shared_lock lock(mutex_);
        auto it = tokens_.find(std::string{token});
        if (it == tokens_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "session token: unknown or revoked"});
        }
        return it->second;
    }

    void
    revoke(std::string_view token) noexcept override {
        std::unique_lock lock(mutex_);
        tokens_.erase(std::string{token});
    }

private:
    std::atomic<std::uint64_t> counter_{0};
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, domain::AccountId> tokens_;
};

}  // namespace pvpgn::infra::inmemory
