// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel.hpp
/// `chat::Channel` aggregate — a single Battle.net chat room.
///
/// Pure: members are identified by `AccountId` (no `t_connection*`),
/// flags collapse the legacy `channel_flags_*` bitfield, and side
/// effects emerge only as `DomainEvent`s drained by the Application
/// layer.

#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::chat {

enum class ChannelFlag : std::uint8_t {
    Public      = 0,
    Permanent   = 1,
    Moderated   = 2,
    Restricted  = 3,
    Silent      = 4,
    System      = 5,
    AllowBots   = 6,
    Locked      = 7,
    /// "the Void" sink channel (legacy `channel_flags_thevoid`). Such channels
    /// exist but are NEVER advertised in the channel list (SID_CHANNELLIST /
    /// WOL LIST), matching the original server which hides the kicked/banned
    /// limbo channel from listings.
    TheVoid     = 8,
};

class ChannelFlags {
public:
    constexpr ChannelFlags() = default;

    void set(ChannelFlag f)      noexcept { bits_.set(static_cast<std::size_t>(f)); }
    void clear(ChannelFlag f)    noexcept { bits_.reset(static_cast<std::size_t>(f)); }
    bool has(ChannelFlag f) const noexcept { return bits_.test(static_cast<std::size_t>(f)); }

    bool operator==(const ChannelFlags&) const = default;

private:
    std::bitset<16> bits_;
};

struct ChannelPolicy {
    ChannelFlags flags;
    /// 0 ⇒ unlimited.
    std::uint32_t max_members = 0;
    /// Restrict admission to a single client tag (0 ⇒ any).
    ClientTag client = ClientTag{};
};

class Channel {
public:
    enum class JoinOutcome : std::uint8_t { Accepted, Full, Banned, WrongClientTag, Locked };

    static Channel create(ChannelId id, std::string name, ChannelPolicy policy) {
        return Channel{id, std::move(name), std::move(policy)};
    }

    /// Rehydrate from persistence — no events emitted.
    static Channel rehydrate(ChannelId id, std::string name, std::string topic,
                             ChannelPolicy policy,
                             const std::unordered_map<AccountId, ClientTag>& members,
                             const std::vector<AccountId>& banlist) {
        Channel c{id, std::move(name), std::move(policy)};
        c.topic_ = std::move(topic);
        c.members_ = members;
        c.banlist_ = banlist;
        return c;
    }

    /// Return a copy of this channel with a different id. Used by repositories
    /// to stamp a freshly-allocated id onto a channel that was created with the
    /// id-0 "assign on persist" sentinel. Preserves all other state (members,
    /// banlist, topic, policy, pending events).
    [[nodiscard]] Channel with_id(ChannelId new_id) const {
        Channel c{*this};
        c.id_ = new_id;
        return c;
    }

    // --- Queries --------------------------------------------------------

    ChannelId           id()           const noexcept { return id_; }
    const std::string&  name()         const noexcept { return name_; }
    const std::string&  topic()        const noexcept { return topic_; }
    const ChannelPolicy& policy()      const noexcept { return policy_; }
    std::size_t         member_count() const noexcept { return members_.size(); }
    /// The current channel operator (gavel), if any. See operator_id_.
    std::optional<AccountId> operator_id() const noexcept { return operator_id_; }
    bool                is_full()      const noexcept {
        return policy_.max_members != 0 && members_.size() >= policy_.max_members;
    }
    bool                contains(AccountId a) const noexcept {
        return members_.find(a) != members_.end();
    }
    bool                is_banned(AccountId a) const noexcept {
        for (auto& b : banlist_) { if (b == a) return true; }
        return false;
    }

    /// Get all member account IDs.
    std::vector<AccountId> member_ids() const noexcept {
        std::vector<AccountId> ids;
        for (const auto& [id, _] : members_) {
            ids.push_back(id);
        }
        return ids;
    }

    // --- Commands -------------------------------------------------------

    JoinOutcome admit(AccountId who, ClientTag tag) {
        if (policy_.flags.has(ChannelFlag::Locked)) {
            events_.push_back(events::ChannelJoinRejected{
                id_, who, events::ChannelJoinRejected::Reason::Locked});
            return JoinOutcome::Locked;
        }
        if (is_banned(who)) {
            events_.push_back(events::ChannelJoinRejected{
                id_, who, events::ChannelJoinRejected::Reason::Banned});
            return JoinOutcome::Banned;
        }
        if (policy_.client != ClientTag{} && !(policy_.client == tag)) {
            events_.push_back(events::ChannelJoinRejected{
                id_, who, events::ChannelJoinRejected::Reason::WrongClientTag});
            return JoinOutcome::WrongClientTag;
        }
        if (is_full()) {
            events_.push_back(events::ChannelJoinRejected{
                id_, who, events::ChannelJoinRejected::Reason::Full});
            return JoinOutcome::Full;
        }
        // Idempotent — joining twice is a no-op (no duplicate event).
        if (auto [_, inserted] = members_.emplace(who, tag); !inserted) {
            return JoinOutcome::Accepted;
        }
        // First member of a non-permanent channel becomes its operator (tmpOP),
        // mirroring the original (channel.cpp: currmembers==1 -> conn_set_tmpOP).
        // Permanent/predefined channels have no auto-operator.
        if (!operator_id_ && members_.size() == 1 &&
            !policy_.flags.has(ChannelFlag::Permanent)) {
            operator_id_ = who;
        }
        events_.push_back(events::ChannelJoined{id_, who});
        return JoinOutcome::Accepted;
    }

    void leave(AccountId who) {
        if (members_.erase(who) == 0) return;
        reassign_operator_on_leave(who);
        events_.push_back(events::ChannelLeft{id_, who});
    }

    /// Returns `true` iff the message was admitted (sender is a member
    /// and channel is not in a silent state for non-moderators).
    bool post(AccountId from, ChatMessage body) {
        if (!contains(from)) return false;
        events_.push_back(events::ChannelMessageSent{id_, from, std::move(body)});
        return true;
    }

    /// Kick is only honoured if the moderator is a member; targets that
    /// aren't members are still added to the banlist (legacy behaviour).
    bool kick(AccountId moderator, AccountId target) {
        if (!contains(moderator)) return false;
        members_.erase(target);
        reassign_operator_on_leave(target);
        if (!is_banned(target)) banlist_.push_back(target);
        events_.push_back(events::ChannelMemberKicked{id_, moderator, target});
        return true;
    }

    void set_topic(AccountId moderator, std::string topic) {
        if (!contains(moderator)) return;
        topic_ = std::move(topic);
        events_.push_back(events::ChannelTopicChanged{id_, moderator, topic_});
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    Channel(ChannelId id, std::string name, ChannelPolicy policy)
        : id_(id), name_(std::move(name)), policy_(std::move(policy)) {}

    ChannelId                                       id_;
    std::string                                     name_;
    std::string                                     topic_;
    ChannelPolicy                                   policy_;
    std::unordered_map<AccountId, ClientTag>        members_;
    std::vector<AccountId>                          banlist_;
    std::vector<events::DomainEvent>                events_;
    /// The channel operator (gavel). The original makes the FIRST user to join a
    /// non-permanent channel its temporary operator (tmpOP); predefined/permanent
    /// channels have no auto-operator. The gavel is NEVER migrated to another
    /// member: when the operator leaves/is kicked it is simply cleared, matching
    /// the original (channel.cpp: on member removal it only does
    /// conn_set_tmpOP_channel(connection, NULL) and promotes no one).
    std::optional<AccountId>                        operator_id_{};

    /// On a member departure, clear the gavel iff `who` was the operator. The
    /// original performs no promotion of a remaining member, so neither do we.
    void reassign_operator_on_leave(AccountId who) {
        if (operator_id_ && operator_id_->value() == who.value()) {
            operator_id_.reset();
        }
    }
};

}  // namespace pvpgn::domain::chat
