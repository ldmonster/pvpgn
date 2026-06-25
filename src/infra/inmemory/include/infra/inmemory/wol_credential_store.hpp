// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_credential_store.hpp
/// In-memory IWolCredentialStore — Westwood Online APGAR token per account name
/// (case-insensitive). Thread-safe; used by the inmemory bnetd backend and by
/// tests. Mirrors [[srp3_credential_store]].

#include <algorithm>
#include <cctype>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "application/auth/wol_credential_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryWolCredentialStore final
    : public application::auth::IWolCredentialStore {
public:
    [[nodiscard]] std::optional<application::auth::WolCredentials>
    find(std::string_view username) const override {
        std::shared_lock lock(mutex_);
        auto it = by_name_.find(key(username));
        if (it == by_name_.end()) return std::nullopt;
        return it->second;
    }

    void store(std::string_view username,
               const application::auth::WolCredentials& creds) override {
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
    std::unordered_map<std::string, application::auth::WolCredentials> by_name_;
};

}  // namespace pvpgn::infra::inmemory
