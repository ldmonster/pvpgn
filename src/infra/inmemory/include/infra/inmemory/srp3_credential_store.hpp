// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file srp3_credential_store.hpp
/// In-memory ISrp3CredentialStore — WarCraft III SRP-3 salt/verifier per
/// account name (case-insensitive). Thread-safe; used by the inmemory bnetd
/// backend and by tests.

#include <algorithm>
#include <cctype>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "application/auth/srp3_credential_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemorySrp3CredentialStore final
    : public application::auth::ISrp3CredentialStore {
public:
    [[nodiscard]] std::optional<application::auth::Srp3Credentials>
    find(std::string_view username) const override {
        std::shared_lock lock(mutex_);
        auto it = by_name_.find(key(username));
        if (it == by_name_.end()) return std::nullopt;
        return it->second;
    }

    void store(std::string_view username,
               const application::auth::Srp3Credentials& creds) override {
        std::unique_lock lock(mutex_);
        by_name_[key(username)] = creds;
    }

private:
    static std::string key(std::string_view name) {
        std::string k{name};
        std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return k;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, application::auth::Srp3Credentials> by_name_;
};

}  // namespace pvpgn::infra::inmemory
