// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_repository.hpp
/// Thread-safe in-memory implementation of IIpBanRepository.
/// Suitable for tests and development.

#include <chrono>
#include <functional>
#include <mutex>
#include <shared_mutex>

#include "domain/moderation/ports.hpp"
#include "domain/moderation/ip_ban_list.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryIpBanRepository final
    : public domain::moderation::IIpBanRepository {
public:
    core::Result<bool>
    is_banned(const domain::IpAddress& ip) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        return banlist_.blocks(ip, now);
    }

    core::Status<>
    add_ban(domain::moderation::IpBanEntry entry) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        banlist_.add(entry);
        return core::ok();
    }

    core::Status<>
    add_range_ban(domain::IpAddress network, std::uint8_t prefix_bits,
                  std::string reason, domain::AccountId issuer,
                  core::SystemTime issued_at,
                  std::optional<core::SystemTime> expires_at) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        banlist_.add_range(network, prefix_bits, reason, issuer, issued_at,
                          expires_at);
        return core::ok();
    }

    core::Status<>
    remove_ban(const domain::IpAddress& ip) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (!banlist_.remove(ip)) {
            return core::fail(core::Error{
                core::StatusCode::NotFound,
                "ip_ban: entry not found"});
        }
        return core::ok();
    }

    core::Status<>
    remove_range_ban(domain::IpAddress network,
                     std::uint8_t prefix_bits) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (!banlist_.remove_range(network, prefix_bits)) {
            return core::fail(core::Error{
                core::StatusCode::NotFound,
                "ip_ban: range not found"});
        }
        return core::ok();
    }

    void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& entry : banlist_.entries()) {
            if (!predicate(entry)) break;
        }
    }

    core::Result<domain::moderation::IpBanList>
    load_banlist() const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return banlist_;
    }

    core::Status<>
    save_banlist(const domain::moderation::IpBanList& banlist) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        banlist_ = banlist;
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    domain::moderation::IpBanList banlist_;
};

}  // namespace pvpgn::infra::inmemory
