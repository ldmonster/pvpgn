// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account.hpp
/// `identity::Account` — the canonical user-account aggregate.
///
/// **Pure**: no I/O, no logger, no scheduler. Commands return events
/// drained by the Application layer. Time is injected as a
/// `core::SystemTime` argument so unit tests stay deterministic.
///
/// Scope of this first cut: the wire-essential invariants — name
/// validity, password change, login (credential check + ban gating),
/// command-group flags, ban issue/clear. Stats, attributes, friends,
/// clan-membership land in follow-up commits as their bounded contexts
/// arrive.

#include <bitset>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ban.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::domain::identity {

/// Bitset of legacy command groups (1..8). Bit `i` set means group
/// `i+1` is granted to this account.
class CommandGroupMask {
public:
    static constexpr std::size_t kBits = 8;

    constexpr CommandGroupMask() = default;

    void grant(std::uint8_t group) noexcept {
        if (group >= 1 && group <= kBits) bits_.set(group - 1);
    }
    void revoke(std::uint8_t group) noexcept {
        if (group >= 1 && group <= kBits) bits_.reset(group - 1);
    }
    bool has(std::uint8_t group) const noexcept {
        return group >= 1 && group <= kBits && bits_.test(group - 1);
    }
    bool any() const noexcept { return bits_.any(); }

    /// True if **any** of groups 7/8 is set — legacy "admin" semantics.
    bool is_admin() const noexcept {
        return bits_.test(6) || bits_.test(7);
    }

    bool operator==(const CommandGroupMask&) const = default;

private:
    std::bitset<kBits> bits_;
};

class Account {
public:
    /// Factory: validates inputs and constructs an account. Emits
    /// `AccountCreated`.
    static core::Result<Account>
    create(AccountId id, UserName name, BNHash password, Locale loc) {
        Account a{id, std::move(name), std::move(password), loc};
        a.events_.push_back(events::AccountCreated{a.id_, a.name_});
        return a;
    }

    /// Rehydrate from persistence -- no events emitted. Caller (the
    /// repository) is responsible for the snapshot's validity.
    static Account rehydrate(AccountId id, UserName name, BNHash password,
                             Locale loc, CommandGroupMask groups,
                             std::optional<Ban> ban, bool locked,
                             bool must_change_password = false) {
        Account a{id, std::move(name), std::move(password), loc};
        a.groups_ = groups;
        a.ban_    = std::move(ban);
        a.locked_ = locked;
        a.must_change_password_ = must_change_password;
        return a;
    }

    // --- Queries (pure) --------------------------------------------------

    AccountId             id()        const noexcept { return id_; }
    const UserName&       name()      const noexcept { return name_; }
    Locale                locale()    const noexcept { return locale_; }
    const CommandGroupMask& command_groups() const noexcept { return groups_; }
    bool                  is_admin()  const noexcept { return groups_.is_admin(); }
    bool                  is_locked() const noexcept { return locked_; }
    const std::optional<Ban>& ban()   const noexcept { return ban_; }

    /// True if the operator (or a password-rotation policy) has
    /// flagged this account: the password matched but the user must
    /// rotate it before a session is granted. The application-layer
    /// `LoginUser` use-case translates this into
    /// `LoginError::MustChangePassword` (Batch 23d).
    bool                  must_change_password() const noexcept {
        return must_change_password_;
    }

    /// True if the account is barred from logging in at the given
    /// wall-clock — covers expired bans correctly.
    bool is_login_barred(core::SystemTime now) const noexcept {
        if (locked_) return true;
        if (ban_ && ban_->active_at(now)) return true;
        return false;
    }

    /// Pure credential check. Used by flows that need to verify
    /// the caller knows the current password *without* the
    /// side-effects of `login()` (no event emission, no ban gating,
    /// no IP / clienttag input). The `ChangePasswordUseCase` is the
    /// first such caller.
    bool verify_password(const BNHash& candidate) const noexcept {
        return password_ == candidate;
    }

    /// Read-only access to the stored password hash1. Required by
    /// flows that re-derive the legacy session-hash transcript
    /// (`bnet_hash(ticks||sessionkey||hash1)`) to verify a hash2
    /// arrived in a CLIENT_LOGINREQ1 / CLIENT_CHANGEPASSREQ
    /// packet -- the application layer routes this through an
    /// `IPasswordHasher` port, never recomputes the algorithm
    /// itself.
    const BNHash& password_hash1() const noexcept { return password_; }

    // --- Commands (mutate + emit events) --------------------------------

    /// Attempt a login. Emits exactly one of
    /// `UserLoggedIn` / `UserLoginRejected`.
    enum class LoginOutcome : std::uint8_t { Accepted, InvalidCredentials, Banned, Locked };
    LoginOutcome login(const BNHash& candidate, IpAddress ip, ClientTag tag,
                       core::SystemTime now) {
        if (locked_) {
            events_.push_back(events::UserLoginRejected{
                id_, events::UserLoginRejected::Reason::AccountLocked, now});
            return LoginOutcome::Locked;
        }
        if (ban_ && ban_->active_at(now)) {
            events_.push_back(events::UserLoginRejected{
                id_, events::UserLoginRejected::Reason::AccountBanned, now});
            return LoginOutcome::Banned;
        }
        if (!(password_ == candidate)) {
            events_.push_back(events::UserLoginRejected{
                id_, events::UserLoginRejected::Reason::InvalidCredentials, now});
            return LoginOutcome::InvalidCredentials;
        }
        // Expired ban: clear and continue.
        if (ban_ && !ban_->active_at(now)) {
            ban_.reset();
            events_.push_back(events::AccountUnbanned{id_});
        }
        events_.push_back(events::UserLoggedIn{id_, ip, tag, now});
        return LoginOutcome::Accepted;
    }

    void change_password(BNHash new_hash) {
        password_ = std::move(new_hash);
        // Successful rotation always clears the "must change" flag.
        const bool was_required = must_change_password_;
        must_change_password_ = false;
        events_.push_back(events::AccountPasswordChanged{id_});
        if (was_required) {
            events_.push_back(events::AccountPasswordRotationCleared{id_});
        }
    }

    /// Force the user to rotate their password on next successful
    /// login. Emits `AccountPasswordRotationRequired` only on the
    /// edge (false -> true); idempotent on a no-op flip.
    void require_password_change() noexcept {
        if (must_change_password_) return;
        must_change_password_ = true;
        try {
            events_.push_back(events::AccountPasswordRotationRequired{id_});
        } catch (...) {
            // event emission is best-effort; the flag has flipped.
        }
    }

    /// Operator override: clear the rotation flag without rotating
    /// the password (e.g. admin "unmark for change"). Emits
    /// `AccountPasswordRotationCleared` only on the edge.
    void clear_password_change_requirement() noexcept {
        if (!must_change_password_) return;
        must_change_password_ = false;
        try {
            events_.push_back(events::AccountPasswordRotationCleared{id_});
        } catch (...) {
        }
    }

    void grant_command_group(std::uint8_t group) {
        if (group < 1 || group > CommandGroupMask::kBits) return;
        if (groups_.has(group)) return;
        groups_.grant(group);
        events_.push_back(events::AccountCommandGroupGranted{id_, group});
    }

    void revoke_command_group(std::uint8_t group) {
        groups_.revoke(group);
    }

    void apply_ban(Ban ban) {
        ban_ = std::move(ban);
        events_.push_back(events::AccountBanned{id_, *ban_});
    }

    void clear_ban() {
        if (!ban_) return;
        ban_.reset();
        events_.push_back(events::AccountUnbanned{id_});
    }

    void lock()   noexcept { locked_ = true; }
    void unlock() noexcept { locked_ = false; }

    /// Move out any pending events. Called by the Application layer
    /// after each command-handler returns.
    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    Account(AccountId id, UserName name, BNHash password, Locale loc)
        : id_(id), name_(std::move(name)), password_(std::move(password)), locale_(loc) {}

    AccountId                          id_{};
    UserName                           name_;
    BNHash                             password_;
    Locale                             locale_;
    CommandGroupMask                   groups_;
    std::optional<Ban>                 ban_;
    bool                               locked_ = false;
    bool                               must_change_password_ = false;
    std::vector<events::DomainEvent>   events_;
};

}  // namespace pvpgn::domain::identity
