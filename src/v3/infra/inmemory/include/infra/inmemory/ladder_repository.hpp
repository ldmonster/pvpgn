// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder_repository.hpp
/// Thread-safe in-memory implementation of ILadderRepository.
/// Maintains per-account, per-ClientTag ladder entries with sorted views
/// for pagination.

#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <functional>
#include <shared_mutex>
#include <vector>

#include "application/ports/ladder_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryLadderRepository final
    : public application::ports::ILadderRepository {
public:
    core::Result<application::ports::LadderEntry>
    find_by_account(domain::AccountId id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_account_.find(id.value());
        if (it == by_account_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound,
                "ladder: no entry for account"});
        }
        // Return the first entry (assumes at least one tag per account)
        if (it->second.empty()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound,
                "ladder: account has no ladder entries"});
        }
        return it->second.begin()->second;
    }

    core::Status<>
    save(const application::ports::LadderEntry& entry) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        by_account_[entry.account_id.value()][entry.name] = entry;
        return core::ok();
    }

    core::Result<std::vector<application::ports::LadderEntry>>
    get_page(domain::ClientTag tag, std::uint32_t offset,
             std::uint32_t count) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        // Collect all entries for this tag
        std::vector<application::ports::LadderEntry> entries;
        for (const auto& [_account_id, tags_map] : by_account_) {
            for (const auto& [_name, entry] : tags_map) {
                entries.push_back(entry);
            }
        }
        
        // Sort by rating descending (higher rating = better rank)
        std::sort(entries.begin(), entries.end(),
                 [](const auto& a, const auto& b) {
                     return a.rating > b.rating;
                 });
        
        // Update ranks based on sorted order
        for (std::uint32_t i = 0; i < entries.size(); ++i) {
            entries[i].rank = i + 1;
        }
        
        // Extract page
        std::vector<application::ports::LadderEntry> page;
        if (offset >= entries.size()) {
            return page;  // Empty page
        }
        std::uint32_t end = std::min(
            static_cast<std::uint32_t>(offset + count),
            static_cast<std::uint32_t>(entries.size()));
        page.assign(entries.begin() + offset,
                   entries.begin() + end);
        return page;
    }

    std::uint32_t
    total_entries(domain::ClientTag tag) const noexcept override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::uint32_t count = 0;
        for (const auto& [_account_id, tags_map] : by_account_) {
            for (const auto& [_name, entry] : tags_map) {
                count++;
            }
        }
        return count;
    }

    void for_each(domain::ClientTag tag,
                  std::function<bool(const application::ports::LadderEntry&)>
                      predicate) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [_account_id, tags_map] : by_account_) {
            for (const auto& [_name, entry] : tags_map) {
                if (!predicate(entry)) return;
            }
        }
    }

private:
    // Structure: AccountId -> (UserName -> LadderEntry)
    // Allows multiple entries per account for different client tags
    struct UserNameHash {
        std::size_t operator()(const domain::UserName& n) const {
            return std::hash<std::string>{}(std::string{n.canonical()});
        }
    };

    struct UserNameEqual {
        bool operator()(const domain::UserName& a,
                       const domain::UserName& b) const {
            return std::string{a.canonical()} == std::string{b.canonical()};
        }
    };

    mutable std::shared_mutex mutex_;
    ankerl::unordered_dense::map<
        std::uint32_t,
        ankerl::unordered_dense::map<domain::UserName,
                                     application::ports::LadderEntry,
                                     UserNameHash, UserNameEqual>>
        by_account_;
};

}  // namespace pvpgn::infra::inmemory
