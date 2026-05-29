// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file team.hpp
/// `social::Team` aggregate — W3 arranged-team ladder roster.
///
/// Fixed-size group (2..4 members for AT). Members are decided at
/// creation; a team is immutable after that — to change rosters you
/// disband and create a new team. This matches the legacy
/// `team.cpp` semantics.

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::social {

class Team {
public:
    static constexpr std::size_t kMinSize = 2;
    static constexpr std::size_t kMaxSize = 4;

    static core::Result<Team> create(TeamId id, std::vector<AccountId> members,
                                     ClientTag client) {
        if (members.size() < kMinSize || members.size() > kMaxSize) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "team must have 2..4 members"});
        }
        // No duplicate members.
        auto sorted = members;
        std::sort(sorted.begin(), sorted.end());
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "team members must be unique"});
        }
        Team t{id, std::move(members), client};
        t.events_.push_back(events::TeamCreated{id, t.members_, client});
        return t;
    }

    TeamId                          id()      const noexcept { return id_; }
    ClientTag                       client()  const noexcept { return client_; }
    const std::vector<AccountId>&   members() const noexcept { return members_; }
    std::size_t                     size()    const noexcept { return members_.size(); }
    bool                            disbanded() const noexcept { return disbanded_; }

    bool contains(AccountId a) const noexcept {
        return std::find(members_.begin(), members_.end(), a) != members_.end();
    }

    /// One-way transition.
    void disband() {
        if (disbanded_) return;
        disbanded_ = true;
        events_.push_back(events::TeamDisbanded{id_});
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    Team(TeamId id, std::vector<AccountId> members, ClientTag client)
        : id_(id), members_(std::move(members)), client_(client) {}

    TeamId                              id_;
    std::vector<AccountId>              members_;
    ClientTag                           client_;
    bool                                disbanded_ = false;
    std::vector<events::DomainEvent>    events_;
};

}  // namespace pvpgn::domain::social
