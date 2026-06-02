// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder_repository.hpp
/// Thread-safe in-memory implementation of ILadderRepository.
/// Maintains ladder entries sorted by rating.

#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "domain/ladder/ports.hpp"
#include "domain/ladder/ladder.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryLadderRepository final
    : public domain::ladder::ILadderRepository {
public:
    core::Result<uint32_t, core::Error>
    get_rank(domain::AccountId account_id) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        // Collect all entries and sort by rating
        std::vector<domain::ladder::LadderEntry> entries;
        for (const auto& [_account_id, entry] : by_account_) {
            entries.push_back(entry);
        }

        // Sort by rating descending (higher rating = better rank)
        std::sort(entries.begin(), entries.end(),
                 [](const auto& a, const auto& b) {
                     return a.rating > b.rating;
                 });

        // Find the account and return its rank
        for (std::uint32_t i = 0; i < entries.size(); ++i) {
            if (entries[i].account == account_id) {
                return i + 1;  // Rank is 1-based
            }
        }

        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "ladder: account not found"});
    }

    core::Result<void, core::Error>
    save_entry(const domain::ladder::LadderEntry& entry) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        by_account_[std::to_string(entry.account.value())] = entry;
        return core::ok();
    }

    core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
    get_top_n(uint32_t n) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        // Collect all entries
        std::vector<domain::ladder::LadderEntry> entries;
        for (const auto& [_account_id, entry] : by_account_) {
            entries.push_back(entry);
        }
        
        // Sort by rating descending (higher rating = better rank)
        std::sort(entries.begin(), entries.end(),
                 [](const auto& a, const auto& b) {
                     return a.rating > b.rating;
                 });
        
        // Return top N
        if (n >= entries.size()) {
            return entries;
        }
        entries.resize(n);
        return entries;
    }

private:
    mutable std::shared_mutex mutex_;
    ankerl::unordered_dense::map<std::string,
                                 domain::ladder::LadderEntry> by_account_;
};

}  // namespace pvpgn::infra::inmemory
