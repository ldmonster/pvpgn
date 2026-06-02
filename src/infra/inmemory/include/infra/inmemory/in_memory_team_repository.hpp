// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_team_repository.hpp
/// In-memory fake for ITeamRepository.
/// Suitable for tests and development/CI composition roots.

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "domain/social/ports.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/team.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryTeamRepository final
    : public domain::social::ITeamRepository {
public:
    core::Result<std::shared_ptr<domain::social::Team>, core::Error>
    find_by_id(domain::TeamId id) override {
        std::shared_lock lock(mutex_);
        auto it = teams_.find(id);
        if (it == teams_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "team: id not found"});
        }
        return it->second;
    }

    core::Result<std::vector<std::shared_ptr<domain::social::Team>>, core::Error>
    find_by_member(domain::AccountId account_id) override {
        std::shared_lock lock(mutex_);
        std::vector<std::shared_ptr<domain::social::Team>> result;
        for (const auto& [id, team] : teams_) {
            if (team->contains(account_id)) {
                result.push_back(team);
            }
        }
        return result;
    }

    core::Result<void, core::Error>
    save(const domain::social::Team& team) override {
        std::unique_lock lock(mutex_);
        teams_[team.id()] = std::make_shared<domain::social::Team>(team);
        return core::ok();
    }

    core::Result<void, core::Error>
    remove(domain::TeamId id) override {
        std::unique_lock lock(mutex_);
        auto it = teams_.find(id);
        if (it == teams_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "team: id not found"});
        }
        teams_.erase(it);
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<domain::TeamId,
                       std::shared_ptr<domain::social::Team>> teams_;
};

}  // namespace pvpgn::infra::inmemory
