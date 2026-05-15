// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file whisper.hpp
/// `chat::Whisper` — a private directed message between two accounts.
/// `chat::IgnoreList` — manages which accounts a user ignores.
///
/// Whisper bypasses `Channel` entirely and is tracked separately for:
/// - Ignore list enforcement (sender blocked by recipient)
/// - Rate limiting (per sender quota)
/// - Cross-protocol delivery (BNet whisper → IRC PRIVMSG bridge)
///
/// Pure: events emerge only as `DomainEvent`s drained by the Application layer.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::domain::chat {

/// Whisper value object — represents a single private message.
struct Whisper {
    AccountId       from;
    AccountId       to;
    UserName        from_name;
    UserName        to_name;
    ChatMessage     body;
    core::SystemTime sent_at;
};

/// WhisperOutcome — result of attempting a whisper delivery.
enum class WhisperOutcome : std::uint8_t {
    Delivered,          ///< Message delivered successfully
    RecipientIgnoring,  ///< Recipient has sender on ignore list
    RecipientOffline,   ///< Recipient has no active session
    SenderQuotaExceeded ///< Sender is muted/throttled
};

/// IgnoreList aggregate — manages which accounts a user ignores.
class IgnoreList {
public:
    static IgnoreList create(AccountId owner) {
        return IgnoreList{owner};
    }

    /// Factory to rehydrate from persistence.
    static IgnoreList rehydrate(AccountId owner, std::vector<AccountId> ignored) {
        IgnoreList il{owner};
        il.ignored_ = std::move(ignored);
        // Sort for binary search efficiency
        std::sort(il.ignored_.begin(), il.ignored_.end());
        return il;
    }

    // --- Queries -----------------------------------------------

    AccountId owner() const noexcept { return owner_; }

    /// Check if this list ignores the given account.
    bool ignores(AccountId target) const noexcept {
        return std::binary_search(ignored_.begin(), ignored_.end(), target);
    }

    bool is_empty() const noexcept {
        return ignored_.empty();
    }

    std::size_t size() const noexcept {
        return ignored_.size();
    }

    const std::vector<AccountId>& ignored_list() const noexcept {
        return ignored_;
    }

    // --- Commands -----------------------------------------------

    /// Add a target to the ignore list. Emits IgnoreAdded event.
    /// Returns true if the target was actually added (not already ignored).
    bool add(AccountId target) {
        auto it = std::lower_bound(ignored_.begin(), ignored_.end(), target);
        if (it != ignored_.end() && *it == target) {
            // Already ignored — no-op, no event
            return false;
        }
        ignored_.insert(it, target);
        events_.push_back(events::IgnoreAdded{owner_, target});
        return true;
    }

    /// Remove a target from the ignore list. Emits IgnoreRemoved event.
    /// Returns true if the target was actually removed.
    bool remove(AccountId target) {
        auto it = std::lower_bound(ignored_.begin(), ignored_.end(), target);
        if (it == ignored_.end() || *it != target) {
            // Not on the list — no-op, no event
            return false;
        }
        ignored_.erase(it);
        events_.push_back(events::IgnoreRemoved{owner_, target});
        return true;
    }

    /// Drain pending events. Called by Application layer after each command.
    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    IgnoreList(AccountId owner) : owner_(owner) {}

    AccountId                       owner_;
    std::vector<AccountId>          ignored_;       // sorted for binary search
    std::vector<events::DomainEvent> events_;
};

}  // namespace pvpgn::domain::chat
