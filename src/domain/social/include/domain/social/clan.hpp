// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan.hpp
/// `social::Clan` aggregate — fixed-rank guild structure.
///
/// Ranks mirror legacy `clan_member_status`:
///   * Chieftain (1)
///   * Shaman    (2 — full officer)
///   * Grunt     (3 — accepted member)
///   * Peon      (4 — invited / probation)

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::social {

enum class ClanRank : std::uint8_t {
    Chieftain = 1,
    Shaman    = 2,
    Grunt     = 3,
    Peon      = 4,
};

struct ClanMember {
    AccountId account;
    ClanRank  rank;
};

class Clan {
public:
    static constexpr std::size_t kMaxMembers = 250;

    /// Clan tag is 2..4 printable ASCII (legacy `MAX_CLANTAG_LEN = 4`).
    static core::Result<Clan> create(ClanId id, std::string tag, std::string name,
                                     AccountId founder, ClientTag client) {
        if (tag.size() < 2 || tag.size() > 4) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "clan tag must be 2..4 chars"});
        }
        for (char ch : tag) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (c < 0x21 || c > 0x7E) {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument, "clan tag must be printable ASCII"});
            }
        }
        if (name.empty() || name.size() > 25) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "clan name must be 1..25 chars"});
        }
        Clan c{id, std::move(tag), std::move(name), client};
        c.members_.push_back({founder, ClanRank::Chieftain});
        c.events_.push_back(events::ClanCreated{id, founder});
        c.events_.push_back(events::ClanMemberJoined{id, founder});
        return c;
    }

    static Clan rehydrate(ClanId id, std::string tag, std::string name,
                          ClientTag client, std::vector<ClanMember> members) {
        Clan c{id, std::move(tag), std::move(name), client};
        c.members_ = std::move(members);
        return c;
    }

    ClanId                          id()      const noexcept { return id_; }
    const std::string&              tag()     const noexcept { return tag_; }
    const std::string&              name()    const noexcept { return name_; }
    ClientTag                       client()  const noexcept { return client_; }
    const std::vector<ClanMember>&  members() const noexcept { return members_; }
    std::size_t                     size()    const noexcept { return members_.size(); }
    bool is_full() const noexcept { return members_.size() >= kMaxMembers; }

    bool contains(AccountId a) const noexcept {
        return find_const_(a) != members_.end();
    }

    enum class JoinOutcome : std::uint8_t { Joined, AlreadyMember, Full };
    JoinOutcome join(AccountId a, ClanRank initial = ClanRank::Peon) {
        if (contains(a)) return JoinOutcome::AlreadyMember;
        if (is_full())   return JoinOutcome::Full;
        members_.push_back({a, initial});
        events_.push_back(events::ClanMemberJoined{id_, a});
        return JoinOutcome::Joined;
    }

    bool remove(AccountId a) {
        auto it = find_mut_(a);
        if (it == members_.end()) return false;
        members_.erase(it);
        events_.push_back(events::ClanMemberLeft{id_, a});
        return true;
    }

    bool set_rank(AccountId a, ClanRank r) {
        auto it = find_mut_(a);
        if (it == members_.end()) return false;
        it->rank = r;
        return true;
    }

    /// Outcome of an authorization-checked rank change.
    enum class PromoteOutcome : std::uint8_t {
        Promoted,
        NotAuthorized,    ///< the promoter is not a Chieftain
        TargetNotMember,  ///< the target is not in this clan
    };

    /// Change `target`'s rank to `new_rank` on behalf of `promoter`, enforcing
    /// the clan invariant that **only a Chieftain may change ranks**. This rule
    /// lives in the aggregate, not in the application use-case (DDD: invariants
    /// belong where the data lives).
    PromoteOutcome promote_member(AccountId promoter, AccountId target,
                                  ClanRank new_rank) {
        auto pit = find_const_(promoter);
        if (pit == members_.end() || pit->rank != ClanRank::Chieftain) {
            return PromoteOutcome::NotAuthorized;
        }
        if (!set_rank(target, new_rank)) {
            return PromoteOutcome::TargetNotMember;
        }
        return PromoteOutcome::Promoted;
    }

    /// Outcome of an authorization-checked kick.
    enum class KickOutcome : std::uint8_t {
        Kicked,
        KickerNotMember,      ///< the kicker is not in this clan
        InsufficientRank,     ///< the kicker is below Shaman
        TargetNotMember,      ///< the target is not in this clan
        CannotKickChieftain,  ///< the Chieftain cannot be kicked
    };

    /// Remove `target` from the clan on behalf of `kicker`, enforcing the clan
    /// rules that a kicker must be Shaman-or-above and that the Chieftain cannot
    /// be kicked. These invariants live in the aggregate, not the use-case.
    KickOutcome kick_member(AccountId kicker, AccountId target) {
        auto kit = find_const_(kicker);
        if (kit == members_.end())          return KickOutcome::KickerNotMember;
        if (kit->rank > ClanRank::Shaman)   return KickOutcome::InsufficientRank;
        auto tit = find_const_(target);
        if (tit == members_.end())          return KickOutcome::TargetNotMember;
        if (tit->rank == ClanRank::Chieftain) return KickOutcome::CannotKickChieftain;
        remove(target);  // emits ClanMemberLeft
        return KickOutcome::Kicked;
    }

    /// Maximum message-of-the-day length (legacy clan MOTD limit).
    static constexpr std::size_t kMaxMotdLen = 256;

    /// Outcome of an authorization-checked MOTD change.
    enum class MotdOutcome : std::uint8_t {
        Set,
        SetterNotMember,   ///< the setter is not in this clan
        InsufficientRank,  ///< the setter is below Shaman
        TooLong,           ///< the MOTD exceeds kMaxMotdLen
    };

    /// Set the clan message-of-the-day on behalf of `setter`, enforcing the
    /// clan rules (setter must be Shaman+, MOTD within length). The authority
    /// and length invariants — and the MOTD state — live in the aggregate.
    MotdOutcome set_motd(AccountId setter, std::string_view text) {
        auto sit = find_const_(setter);
        if (sit == members_.end())        return MotdOutcome::SetterNotMember;
        if (sit->rank > ClanRank::Shaman) return MotdOutcome::InsufficientRank;
        if (text.size() > kMaxMotdLen)    return MotdOutcome::TooLong;
        motd_ = std::string{text};
        return MotdOutcome::Set;
    }

    [[nodiscard]] const std::string& motd() const noexcept { return motd_; }

    /// True iff `a` is a member of this clan with Chieftain rank. A domain query
    /// so callers (e.g. the disband use-case, whose removal is a repository
    /// concern) need not reach into `members()` to check authority.
    [[nodiscard]] bool is_chieftain(AccountId a) const noexcept {
        auto it = find_const_(a);
        return it != members_.end() && it->rank == ClanRank::Chieftain;
    }

    /// Outcome of an authorization-checked invitation.
    enum class InviteOutcome : std::uint8_t {
        Invited,
        InviterNotMember,   ///< the inviter is not in this clan
        InsufficientRank,   ///< the inviter is below Shaman
        AlreadyMember,      ///< the invitee is already in the clan
        Full,               ///< the clan is at capacity
    };

    /// Admit `invitee` (as a Peon) on behalf of `inviter`, enforcing the clan
    /// rule that an inviter must be Shaman+. Membership/capacity are delegated
    /// to `join`. The authority invariant lives in the aggregate.
    InviteOutcome invite_member(AccountId inviter, AccountId invitee) {
        auto iit = find_const_(inviter);
        if (iit == members_.end())        return InviteOutcome::InviterNotMember;
        if (iit->rank > ClanRank::Shaman) return InviteOutcome::InsufficientRank;
        switch (join(invitee, ClanRank::Peon)) {
            case JoinOutcome::AlreadyMember: return InviteOutcome::AlreadyMember;
            case JoinOutcome::Full:          return InviteOutcome::Full;
            case JoinOutcome::Joined:        return InviteOutcome::Invited;
        }
        return InviteOutcome::Invited;  // unreachable: all JoinOutcomes handled
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    Clan(ClanId id, std::string tag, std::string name, ClientTag client)
        : id_(id), tag_(std::move(tag)), name_(std::move(name)), client_(client) {}

    std::vector<ClanMember>::iterator find_mut_(AccountId a) {
        return std::find_if(members_.begin(), members_.end(),
                            [a](const ClanMember& m) { return m.account == a; });
    }
    std::vector<ClanMember>::const_iterator find_const_(AccountId a) const {
        return std::find_if(members_.begin(), members_.end(),
                            [a](const ClanMember& m) { return m.account == a; });
    }

    ClanId                              id_;
    std::string                         tag_;
    std::string                         name_;
    ClientTag                           client_;
    std::string                         motd_;
    std::vector<ClanMember>             members_;
    std::vector<events::DomainEvent>    events_;
};

}  // namespace pvpgn::domain::social
