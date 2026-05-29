// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file anon_game_queue.hpp
/// `matchmaking::AnonGameQueue` — FIFO matchmaking queue for the
/// W3/SC2 "anonymous game" feature. One queue per (`ClientTag`,
/// `team_size`); matchmaking picks `2 * team_size` players whose
/// queue-wait windows overlap.

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/clock.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::matchmaking {

struct QueueEntry {
    AccountId        account;
    core::SystemTime enqueued_at;
};

class AnonGameQueue {
public:
    AnonGameQueue(ClientTag client, std::uint8_t team_size)
        : client_(client), team_size_(team_size) {}

    ClientTag    client()    const noexcept { return client_; }
    std::uint8_t team_size() const noexcept { return team_size_; }
    std::size_t  size()      const noexcept { return entries_.size(); }
    const std::vector<QueueEntry>& entries() const noexcept { return entries_; }

    /// Returns false if `account` is already in the queue.
    bool enqueue(AccountId account, core::SystemTime now) {
        if (in_queue_.contains(account)) return false;
        in_queue_.insert(account);
        entries_.push_back({account, now});
        events_.push_back(events::AnonGameQueued{account, client_, team_size_, now});
        return true;
    }

    bool dequeue(AccountId account) {
        auto it = std::find_if(entries_.begin(), entries_.end(),
                               [&](const QueueEntry& e) { return e.account == account; });
        if (it == entries_.end()) return false;
        entries_.erase(it);
        in_queue_.erase(account);
        events_.push_back(events::AnonGameDequeued{account});
        return true;
    }

    /// True if at least `2 * team_size` players are waiting.
    bool can_match() const noexcept {
        return entries_.size() >= 2u * static_cast<std::size_t>(team_size_);
    }

    /// Drain `2*team_size` oldest entries and emit `AnonGameMatched`.
    /// Caller supplies the `GameId` — game allocation is an Application
    /// concern.
    std::vector<AccountId> match(GameId game) {
        const std::size_t n = 2u * static_cast<std::size_t>(team_size_);
        if (entries_.size() < n) return {};
        std::vector<AccountId> picked;
        picked.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            picked.push_back(entries_[i].account);
            in_queue_.erase(entries_[i].account);
        }
        entries_.erase(entries_.begin(),
                       entries_.begin() + static_cast<std::ptrdiff_t>(n));
        events_.push_back(events::AnonGameMatched{game, picked, client_});
        return picked;
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    ClientTag                                                  client_;
    std::uint8_t                                               team_size_;
    std::vector<QueueEntry>                                    entries_;
    std::unordered_set<AccountId>                              in_queue_;
    std::vector<events::DomainEvent>                           events_;
};

}  // namespace pvpgn::domain::matchmaking
