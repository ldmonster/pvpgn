// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_repository.hpp
/// Thread-safe in-memory implementation of IClanRepository.
/// Suitable for tests and development. Uses dual indexes for O(1) lookup
/// by ID and case-insensitive tag.

#include <ankerl/unordered_dense.h>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

#include "domain/social/ports.hpp"
#include "domain/social/clan.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryClanRepository final
    : public domain::social::IClanRepository {
public:
    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_id(domain::ClanId id) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(
                core::Error{core::StatusCode::NotFound, "clan: id not found"});
        }
        return it->second;
    }

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_tag(std::string_view tag) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        // Normalize tag to lowercase for case-insensitive lookup
        std::string normalized_tag(tag);
        std::transform(normalized_tag.begin(), normalized_tag.end(),
                      normalized_tag.begin(), ::tolower);
        auto it = by_tag_.find(normalized_tag);
        if (it == by_tag_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "clan: tag not found"});
        }
        auto clan_it = by_id_.find(it->second);
        if (clan_it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::Internal, "clan: index corrupt"});
        }
        return clan_it->second;
    }

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_name(std::string_view name) override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [_id, clan] : by_id_) {
            if (clan->name() == name) {
                return clan;
            }
        }
        return core::fail(core::Error{
            core::StatusCode::NotFound, "clan: name not found"});
    }

    core::Result<void, core::Error>
    save(const domain::social::Clan& clan) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_shared<domain::social::Clan>(clan);
        
        // Update tag index (normalized)
        std::string normalized_tag(clan.tag());
        std::transform(normalized_tag.begin(), normalized_tag.end(),
                      normalized_tag.begin(), ::tolower);
        by_tag_[normalized_tag] = clan.id().value();
        
        // Update ID index
        by_id_[clan.id().value()] = copy;
        return core::ok();
    }

    core::Result<void, core::Error>
    remove(std::string_view tag) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        // Normalize tag to lowercase for case-insensitive lookup
        std::string normalized_tag(tag);
        std::transform(normalized_tag.begin(), normalized_tag.end(),
                      normalized_tag.begin(), ::tolower);
        auto it = by_tag_.find(normalized_tag);
        if (it == by_tag_.end()) {
            return core::fail(
                core::Error{core::StatusCode::NotFound, "clan: tag not found"});
        }
        
        // Remove from ID index
        by_id_.erase(it->second);
        
        // Remove from tag index
        by_tag_.erase(it);
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    ankerl::unordered_dense::map<std::uint32_t,
                                 std::shared_ptr<domain::social::Clan>>
        by_id_;
    ankerl::unordered_dense::map<std::string, std::uint32_t> by_tag_;
};

}  // namespace pvpgn::infra::inmemory
