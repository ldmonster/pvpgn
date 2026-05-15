// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_repository.hpp
/// Thread-safe in-memory implementation of IIpBanRepository.
/// Suitable for tests and development.

#include <ankerl/unordered_dense.h>
#include <functional>
#include <shared_mutex>
#include <vector>

#include "application/ports/ip_ban_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryIpBanRepository final
    : public application::ports::IIpBanRepository {
public:
    core::Result<bool>
    is_banned(const domain::IpAddress& ip) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& entry : entries_) {
            // Simple exact match for now; CIDR matching would be more complex
            if (entry.ip == ip) {
                // Check if expired
                if (entry.expires_at) {
                    auto now = core::SystemTime::now();
                    if (now >= *entry.expires_at) {
                        continue;  // Expired
                    }
                }
                return true;
            }
        }
        return false;
    }

    core::Status<>
    add_ban(domain::moderation::IpBanEntry entry) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        // Remove existing ban for this IP
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                          [&entry](const auto& e) { return e.ip == entry.ip; }),
            entries_.end());
        entries_.push_back(entry);
        return core::ok();
    }

    core::Status<>
    add_range_ban(domain::IpAddress network, std::uint8_t prefix_bits,
                  std::string reason, domain::AccountId issuer,
                  core::SystemTime issued_at,
                  std::optional<core::SystemTime> expires_at) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        domain::moderation::IpBanEntry entry{
            .ip = network,
            .prefix_bits = prefix_bits,
            .reason = reason,
            .issuer = issuer,
            .issued_at = issued_at,
            .expires_at = expires_at};
        entries_.push_back(entry);
        return core::ok();
    }

    core::Status<>
    remove_ban(const domain::IpAddress& ip) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = std::find_if(
            entries_.begin(), entries_.end(),
            [&ip](const auto& entry) { return entry.ip == ip; });
        if (it == entries_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound,
                "ip_ban: entry not found"});
        }
        entries_.erase(it);
        return core::ok();
    }

    core::Status<>
    remove_range_ban(domain::IpAddress network,
                     std::uint8_t prefix_bits) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = std::find_if(
            entries_.begin(), entries_.end(),
            [&network, prefix_bits](const auto& entry) {
                return entry.ip == network && entry.prefix_bits == prefix_bits;
            });
        if (it == entries_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound,
                "ip_ban: range not found"});
        }
        entries_.erase(it);
        return core::ok();
    }

    void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> predicate)
        const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& entry : entries_) {
            if (!predicate(entry)) break;
        }
    }

    core::Result<domain::moderation::IpBanList>
    load_banlist() const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        domain::moderation::IpBanList banlist;
        for (const auto& entry : entries_) {
            banlist.add_entry(entry);
        }
        return banlist;
    }

    core::Status<>
    save_banlist(const domain::moderation::IpBanList& banlist) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        entries_.clear();
        banlist.for_each([this](const auto& entry) {
            entries_.push_back(entry);
            return true;
        });
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    std::vector<domain::moderation::IpBanEntry> entries_;
};

}  // namespace pvpgn::infra::inmemory
