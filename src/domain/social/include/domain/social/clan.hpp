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
        RankNotAllowed,   ///< the requested rank may not be set this way (Chieftain)
    };

    /// Change `target`'s rank to `new_rank` on behalf of `promoter`, enforcing
    /// the clan invariants that **only a Chieftain may change ranks** and that
    /// the rank-update path may never mint a second Chieftain. The legacy server
    /// caps this path at `CLAN_PEON..CLAN_SHAMAN` (`handle_bnet.cpp:5133`): the
    /// crown is moved *only* via `transfer_chieftain`, which keeps the count at
    /// exactly one. These rules live in the aggregate, not in the application
    /// use-case (DDD: invariants belong where the data lives).
    PromoteOutcome promote_member(AccountId promoter, AccountId target,
                                  ClanRank new_rank) {
        auto pit = find_const_(promoter);
        if (pit == members_.end() || pit->rank != ClanRank::Chieftain) {
            return PromoteOutcome::NotAuthorized;
        }
        // The rank-update path may NOT create a second Chieftain. Promotion to
        // Chieftain only ever happens via the atomic `transfer_chieftain` crown
        // hand-off, which preserves "exactly one Chieftain".
        if (new_rank == ClanRank::Chieftain) {
            return PromoteOutcome::RankNotAllowed;
        }
        if (!set_rank(target, new_rank)) {
            return PromoteOutcome::TargetNotMember;
        }
        return PromoteOutcome::Promoted;
    }

    /// Outcome of an authorization-checked crown transfer.
    enum class TransferChieftainOutcome : std::uint8_t {
        Transferred,
        NotAuthorized,    ///< the caller is not the current Chieftain
        TargetNotMember,  ///< the target is not in this clan
        TargetIsSelf,     ///< the Chieftain cannot hand the crown to themselves
    };

    /// Atomically move the Chieftain crown from `current` to `target`: the
    /// current Chieftain is demoted to Grunt and `target` is promoted to
    /// Chieftain in a single operation, mirroring the legacy
    /// `_client_clan_membernewchiefreq` handler (`handle_bnet.cpp:5228`). This
    /// is the ONLY way a member becomes Chieftain, and it preserves the
    /// "exactly one Chieftain" invariant by construction (one out, one in).
    TransferChieftainOutcome transfer_chieftain(AccountId current,
                                                AccountId target) {
        auto cit = find_mut_(current);
        if (cit == members_.end() || cit->rank != ClanRank::Chieftain) {
            return TransferChieftainOutcome::NotAuthorized;
        }
        if (current == target) {
            return TransferChieftainOutcome::TargetIsSelf;
        }
        auto tit = find_mut_(target);
        if (tit == members_.end()) {
            return TransferChieftainOutcome::TargetNotMember;
        }
        cit->rank = ClanRank::Grunt;       // old chieftain steps down
        tit->rank = ClanRank::Chieftain;   // new chieftain crowned
        return TransferChieftainOutcome::Transferred;
    }

    /// Number of members holding Chieftain rank. The aggregate invariant for a
    /// created clan is that this is always exactly 1.
    [[nodiscard]] std::size_t chieftain_count() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            members_.begin(), members_.end(),
            [](const ClanMember& m) { return m.rank == ClanRank::Chieftain; }));
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

    /// Outcome of a member leaving the clan.
    enum class LeaveOutcome : std::uint8_t {
        Left,
        NotMember,             ///< the account is not in this clan
        ChieftainMustDisband,  ///< the Chieftain may not just leave
    };

    /// Remove `account` from the clan on their own behalf, enforcing the clan
    /// rule that the Chieftain must disband rather than leave. Invariant lives
    /// in the aggregate.
    LeaveOutcome leave(AccountId account) {
        auto it = find_const_(account);
        if (it == members_.end())            return LeaveOutcome::NotMember;
        if (it->rank == ClanRank::Chieftain) return LeaveOutcome::ChieftainMustDisband;
        remove(account);  // emits ClanMemberLeft
        return LeaveOutcome::Left;
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
