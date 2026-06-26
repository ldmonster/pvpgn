// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file user_profile_store.hpp
/// In-memory IUserProfileStore — per-account string attributes (BNCS profile /
/// user data). Thread-safe; used by the inmemory bnetd backend and tests.
/// Mirrors the other inmemory account-scoped stores.

#include <algorithm>
#include <cctype>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "application/auth/user_profile_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryUserProfileStore final
    : public application::auth::IUserProfileStore {
public:
    void set(std::string_view account, std::string_view key,
             std::string_view value) override {
        std::unique_lock lock(mutex_);
        by_account_[key_of(account)][std::string{key}] = std::string{value};
    }

    [[nodiscard]] std::optional<std::string>
    get(std::string_view account, std::string_view key) const override {
        std::shared_lock lock(mutex_);
        auto ai = by_account_.find(key_of(account));
        if (ai == by_account_.end()) return std::nullopt;
        auto ki = ai->second.find(std::string{key});
        if (ki == ai->second.end()) return std::nullopt;
        return ki->second;
    }

private:
    static std::string key_of(std::string_view name) {
        std::string k{name};
        std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return k;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>>
        by_account_;
};

}  // namespace pvpgn::infra::inmemory
