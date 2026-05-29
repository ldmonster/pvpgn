// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file quota.hpp
/// `moderation::Quota` — sliding-window message rate limiter used by
/// chat to mute spammers. Mirrors legacy `quota.conf` semantics:
/// `lines` messages per `time_window` seconds.
///
/// Pure: caller supplies `core::SystemTime` for every check; no
/// internal clock.

#include <cstdint>
#include <deque>

#include "core/clock.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::moderation {

struct QuotaPolicy {
    std::uint32_t              limit     = 5;   // messages per window
    std::chrono::milliseconds  window    = std::chrono::seconds(2);
    std::chrono::milliseconds  mute_for  = std::chrono::seconds(30);
};

class Quota {
public:
    Quota(AccountId account, QuotaPolicy policy)
        : account_(account), policy_(policy) {}

    enum class Outcome : std::uint8_t { Allowed, Throttled, Muted };

    /// Record one message at `now`. Returns `Throttled` if the message
    /// would exceed the limit (caller can drop it); transitions the
    /// account into `Muted` until `now + mute_for` once exceeded.
    Outcome record(core::SystemTime now) {
        if (muted_until_ && now < *muted_until_) {
            return Outcome::Muted;
        }
        if (muted_until_ && now >= *muted_until_) {
            muted_until_.reset();
            timestamps_.clear();
        }
        evict_old_(now);
        timestamps_.push_back(now);
        if (timestamps_.size() > policy_.limit) {
            muted_until_ = now + policy_.mute_for;
            events_.push_back(events::AccountQuotaExceeded{
                account_,
                static_cast<std::uint32_t>(policy_.window.count()),
                policy_.limit});
            return Outcome::Throttled;
        }
        return Outcome::Allowed;
    }

    bool is_muted(core::SystemTime now) const noexcept {
        return muted_until_.has_value() && now < *muted_until_;
    }

    AccountId account() const noexcept { return account_; }
    std::size_t recent_count() const noexcept { return timestamps_.size(); }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    void evict_old_(core::SystemTime now) {
        const auto cutoff = now - policy_.window;
        while (!timestamps_.empty() && timestamps_.front() < cutoff) {
            timestamps_.pop_front();
        }
    }

    AccountId                              account_;
    QuotaPolicy                            policy_;
    std::deque<core::SystemTime>           timestamps_;
    std::optional<core::SystemTime>        muted_until_;
    std::vector<events::DomainEvent>       events_;
};

}  // namespace pvpgn::domain::moderation
