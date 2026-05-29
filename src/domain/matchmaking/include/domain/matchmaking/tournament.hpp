// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tournament.hpp
/// `matchmaking::Tournament` — simple single-elimination bracket.
/// Round-by-round results are reported externally; this aggregate
/// owns the bracket topology and progression invariants.

#include <cstdint>
#include <utility>
#include <vector>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::matchmaking {

class Tournament {
public:
    static core::Result<Tournament>
    schedule(std::uint32_t id, ClientTag client,
             std::vector<AccountId> participants, core::SystemTime start_at) {
        if (participants.size() < 2) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "tournament needs at least 2 participants"});
        }
        // Single-elim brackets prefer power-of-two participant counts.
        // Non-power-of-two is still allowed; the first round just has byes.
        Tournament t{id, client, std::move(participants), start_at};
        t.events_.push_back(events::TournamentScheduled{id, client, start_at});
        return t;
    }

    std::uint32_t            id()           const noexcept { return id_; }
    ClientTag                client()       const noexcept { return client_; }
    core::SystemTime         start_at()     const noexcept { return start_at_; }
    const std::vector<AccountId>& participants() const noexcept { return participants_; }
    std::size_t              participant_count() const noexcept { return participants_.size(); }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    Tournament(std::uint32_t id, ClientTag client,
               std::vector<AccountId> participants, core::SystemTime start_at)
        : id_(id), client_(client),
          participants_(std::move(participants)), start_at_(start_at) {}

    std::uint32_t                       id_;
    ClientTag                           client_;
    std::vector<AccountId>              participants_;
    core::SystemTime                    start_at_;
    std::vector<events::DomainEvent>    events_;
};

}  // namespace pvpgn::domain::matchmaking
