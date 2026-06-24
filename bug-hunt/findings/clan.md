# Bug Hunt — Clan subsystem (WAR3 clans)

ORIGINAL (read-only): `/home/cnupt/work/pvpgn-server`
CURRENT v3 (read-only): `/home/cnupt/work/pvpgn`

Scope: ranks, member limits, MOTD, tag/name lengths, invite/join/promote/kick/disband authority, creation-pending semantics.

Key v3 files:
- `src/domain/social/include/domain/social/clan.hpp` (aggregate — most logic lives here)
- `src/domain/social/include/domain/social/clan_tag.hpp`
- `src/protocol/bnet/include/protocol/bnet/clan_wire_types.hpp` (wire constants)
- `src/application/social/src/{create_clan,join_clan,invite_to_clan,promote_clan_member,kick_from_clan,leave_clan,disband_clan,set_clan_motd}.cpp`
- `src/infra/persistence/clan_repository.cpp`

Original references:
- `src/bnetd/clan.h` (rank constants), `src/bnetd/clan.cpp`, `src/bnetd/handle_bnet.cpp` (authority/handlers)
- `src/common/field_sizes.h`, `src/common/setup_before.h`, `conf/bnetd.conf.in`

---

## FINDING 1 — Clan rank enum values are INVERTED vs the wire; no domain→wire mapping exists
**Severity: HIGH** · **Classification: BUG**

**Original** (`src/bnetd/clan.h:96-100`):
```c
#define CLAN_CHIEFTAIN 0x04
#define CLAN_SHAMAN    0x03
#define CLAN_GRUNT     0x02
#define CLAN_PEON      0x01
#define CLAN_NEW       0x00
```
Higher numeric value = higher authority. These bytes go on the wire in the member-list / member-update packets.

**v3** (`src/domain/social/include/domain/social/clan.hpp:27-32`):
```cpp
enum class ClanRank : std::uint8_t {
    Chieftain = 1,
    Shaman    = 2,
    Grunt     = 3,
    Peon      = 4,
};
```
Inverted: higher authority = *lower* numeric value.

The correct wire bytes DO exist separately in `clan_wire_types.hpp:49-53`
(`kRankPeon=0x01 … kRankChieftain=0x04`), but **nothing maps `ClanRank` → `kRank*`**
(grep across `src/protocol` and `src/infra` finds no conversion). Worse, the persistence
layer stores the raw inverted enum value directly:
`src/infra/persistence/clan_repository.cpp:26` rehydrates with
`static_cast<ClanRank>(row.get_int(1))` and line 130 writes
`static_cast<uint8_t>(member.rank)`. So a Chieftain is persisted/loaded as `1` and,
if a member-list reply is ever built straight from the domain enum, the Chieftain would
be transmitted as wire byte `0x01` = **Peon**, and Peon as `0x04` = **Chieftain**.

**Divergence:** Rank byte values diverge from the legacy wire protocol, and there is no
translation layer to reconcile the two. This is a latent wire-protocol break the moment
the domain enum reaches a packet, and it already produces non-legacy values in the DB.

**Proposed fix:** Either (a) make `ClanRank` match the legacy bytes
(`Chieftain=4, Shaman=3, Grunt=2, Peon=1`) and flip the authority comparisons (see Finding 6),
or (b) add an explicit `ClanRank ↔ wire kRank*` mapping used at every serialization and
persistence boundary, and document that the DB stores wire bytes. Option (a) is simplest
and matches the original's "higher = stronger" ordering.

**RESOLUTION (2026-06-25): BUG CONFIRMED + FIXED via option (b).**
Trace confirmed the claim:
- Domain enum `ClanRank` is inverted (Chieftain=1 … Peon=4); all in-aggregate
  authority checks (`rank > ClanRank::Shaman`, `rank == ClanRank::Chieftain`)
  depend on that "lower value = higher authority" ordering, so renumbering was
  rejected (would silently break authority — Finding 6/8 comparisons).
- No `ClanRank → kRank*` mapping existed anywhere (`grep` across `src` empty).
- The wire serialization path (`fsm_clan.cpp`) is currently **stubbed**, so no
  live packet emits a rank yet — but the codec writes `entry.rank` raw, so the
  bug would have triggered the moment the FSM populated a member-list reply from
  the domain enum.
- The **persistence path is live and was storing the raw inverted enum**:
  `clan_repository.cpp` wrote `static_cast<uint8_t>(member.rank)` (Chieftain → DB
  byte 1 = legacy Peon) and rehydrated with `static_cast<ClanRank>(...)`.
Fix:
- Added `src/domain/social/include/domain/social/clan_rank_wire.hpp` —
  header-only `clan_rank_to_wire` / `clan_rank_from_wire` mapping
  (Chieftain↔0x04, Shaman↔0x03, Grunt↔0x02, Peon↔0x01; legacy NEW 0x00 → Peon).
- `clan_repository.cpp` now persists/rehydrates through that mapping, so the DB
  stores legacy wire bytes. Enum ordering (and thus every authority comparison)
  is untouched.
- Tests: pinned each rank to its wire byte and verified the
  higher-authority-still-higher property (`tests/unit/domain/social/social_test.cpp`);
  updated `tests/unit/infra/persistence/sql_clan_repository_test.cpp` to seed/assert
  legacy wire bytes.
The wire serialization boundary (when the FSM is implemented) must likewise call
`clan_rank_to_wire` when filling `ClanMemberEntry::rank` / `ClanMemberUpdate::rank`
/ `ClanInfoReply::rank`.

---

## FINDING 2 — `kMaxMembers = 250` vs original default 50
**Severity: MEDIUM** · **Classification: BUG**

**Original:** member cap is the runtime pref `clan_max_members`, default **50**
(`src/common/setup_before.h:164` `CLAN_DEFAULT_MAX_MEMBERS = 50`;
`conf/bnetd.conf.in:622` `clan_max_members = 50`). Enforced at invite time
(`handle_bnet.cpp:5273` `clan_get_member_count(clan) >= prefs_get_clan_max_members()`).

**v3** (`clan.hpp:42`): `static constexpr std::size_t kMaxMembers = 250;` — hardcoded, and
4× the original default. Used by `is_full()` / `join()` / `invite_member()`.

**Divergence:** Wrong member limit (250 vs 50) and it is no longer configurable.

**Proposed fix:** Drive the cap from configuration (default 50) instead of a hardcoded 250,
or at minimum set the constant to 50 to match the shipped default.

---

## FINDING 3 — MOTD max length 256 vs original 224 (223 usable)
**Severity: MEDIUM** · **Classification: BUG**

**Original** (`src/common/field_sizes.h:37`): `CLAN_MOTD_MAX = 224; /* including terminating NUL */`
→ **223 usable characters**. The handler reads the MOTD with this bound
(`clan.cpp:430` `packet_get_str_const(packet, offset, CLAN_MOTD_MAX)`).

**v3** (`clan.hpp:165`): `static constexpr std::size_t kMaxMotdLen = 256;` and
`set_motd` rejects only `text.size() > kMaxMotdLen`.

**Divergence:** MOTD length limit too large (256 vs 223 usable). A 224–256-char MOTD that
v3 accepts would overflow / be truncated against the legacy 224-byte field on the wire.

**Proposed fix:** Set `kMaxMotdLen = 223` (or 224 including NUL semantics, matching how the
length is compared) to match `CLAN_MOTD_MAX`.

---

## FINDING 4 — Clan name length: v3 allows 1..25; original is max 23 usable (CLAN_NAME_MAX=24)
**Severity: MEDIUM** · **Classification: BUG**

**Original** (`src/common/field_sizes.h:36`): `CLAN_NAME_MAX = 24; /* including terminating NUL */`
→ **max 23 usable characters**. Name is read with this bound in the create path
(`handle_bnet.cpp:4956` `packet_get_str_const(packet, offset, CLAN_NAME_MAX)`).

**v3**: `clan.hpp:57` and `create_clan.cpp:18` both enforce `name.size() > 25` (i.e. allow up to 25).

**Divergence:** Upper bound 25 vs 23. Names of 24–25 chars accepted by v3 exceed the legacy
field. (Lower bound: v3 requires ≥1 char; original effectively requires non-NULL non-empty,
so the min matches in spirit.)

**Proposed fix:** Cap name at 23 chars (`CLAN_NAME_MAX - 1`).

---

## FINDING 5 — Clan tag minimum length 2 vs original 1
**Severity: MEDIUM** · **Classification: BUG**

**Original:** a clan tag is a 4-byte `t_tag`. `str_to_clantag` (`clan.cpp:1473`) accepts
**1 to 4** characters — a single non-NUL char (`str[0]`) already yields a valid tag. There is
no minimum-of-2 rule anywhere.

**v3** (`clan.hpp:46-49` and `clan_tag.hpp:21,40`): requires `tag.size() < 2 || > 4` → rejects
1-char tags. `create_clan.cpp:15` repeats the `tag.size() < 2` check.

**Divergence:** v3 rejects valid 1-character clan tags that the original accepts (off-by-one
on the minimum). Max (4) matches.

Minor sub-note: `clan.hpp` validates tag chars as `0x21..0x7E` (excludes space 0x20) while
`clan_tag.hpp::parse` allows `0x20..0x7E` (includes space). Inconsistent within v3; the
original imposes no printable-range filter at all. Low severity.

**Proposed fix:** Set `kMinLen = 1` for clan tags in both `clan.hpp::create` and
`ClanTag`/`create_clan.cpp`.

---

## FINDING 6 — Shaman cannot promote/demote at all; original lets Shaman manage lower ranks
**Severity: HIGH** · **Classification: BUG**

**Original** rank-update authority (`handle_bnet.cpp:5133-5141`):
```c
if ((status < CLAN_PEON) || (status > CLAN_SHAMAN)) { ...FAILED; }   // target rank must be PEON..SHAMAN
else if ((((clanmember_get_status(member) == CLAN_SHAMAN)
            && (status < CLAN_SHAMAN)
            && (clanmember_get_status(dest_member) < CLAN_SHAMAN)) ||
          (clanmember_get_status(member) == CLAN_CHIEFTAIN))
         && (clanmember_set_status(dest_member, status) == 0)) { ...SUCCESS; }
```
Rules: (a) target rank must be within Peon..Shaman — **nobody may be promoted to Chieftain**
via rank update, and CLAN_NEW cannot be set; (b) a **Shaman** may change a member's rank only
to *below* Shaman and only when the target is currently *below* Shaman (i.e. manage Grunts/Peons,
not other Shamans/the Chieftain); (c) a **Chieftain** may change any rank within those bounds.

**v3** `Clan::promote_member` (`clan.hpp:122-133`) + `promote_clan_member.cpp`:
```cpp
if (pit == members_.end() || pit->rank != ClanRank::Chieftain)
    return PromoteOutcome::NotAuthorized;   // ONLY a Chieftain may change ranks
... set_rank(target, new_rank);
```
And `promote_clan_member.cpp:31-33` accepts `"shaman"`/`"chieftain"` as targets with no
upper bound.

**Divergence (two bugs):**
1. **Authority too strict:** Shamans are completely barred from promoting/demoting, but the
   original explicitly allows a Shaman to manage members below Shaman rank.
2. **Bound missing:** v3 allows promoting a target to **Chieftain** (and to **Shaman**),
   whereas the original forbids promoting anyone to Chieftain via this path and caps the
   target rank at Shaman. (Chieftain transfer in the original is a separate
   `CLIENT_CLAN_MEMBERNEWCHIEFREQ` flow, `handle_bnet.cpp:5228`, where the old chief is demoted
   to Grunt and the new one set to Chieftain.)

**Proposed fix:** In `promote_member`, allow Chieftain (any target rank Peon..Shaman) AND
Shaman (target rank strictly below Shaman, and target currently below Shaman); reject any
attempt to set the target to Chieftain or to NEW. Keep the dedicated new-chief transfer
separate.

---

## FINDING 7 — No "creation pending" / probation (`created`) semantics in v3
**Severity: MEDIUM** · **Classification: BUG (missing parity feature)**

**Original** models a two-phase clan creation. On `clan_create` the clan starts with
`created = 0` (`clan.cpp` create body). The create-invite handler sets
`clan_set_created(clan, -membercount)` (`handle_bnet.cpp:4981`); each invitee that accepts
increments it (`handle_bnet.cpp:5065-5083`); the clan only becomes officially "created"
(`clan_set_created(clan, 1)`, creation_time set, `clan_save`) once all invited members have
accepted (counter reaches 0). The required invite count has a configured minimum
`clan_min_invites`, default **2** (`setup_before.h:168` `CLAN_DEFAULT_MIN_INVITES = 2`).
Separately, a "newer" probation exists: new members start at `CLAN_NEW` and are auto-promoted
to `CLAN_PEON` after `clan_newer_time` hours (`clan.cpp:1009-1011`).

**v3** `Clan::create` (`clan.hpp:44-65`) immediately produces a fully-formed clan with **only
the founder** as Chieftain, emits `ClanCreated`, and persists right away
(`create_clan.cpp`). There is no `created` counter, no minimum-invites-to-form rule, no
`CLAN_NEW` rank used, and no probation auto-promotion. `ClanRank::Peon` is the lowest rank
modeled and `kRankNew (0x00)` is never used in the domain.

**Divergence:** v3 lets a single account form a clan instantly, bypassing the original's
"need ≥ clan_min_invites accepted members before the clan exists" rule and the NEW→PEON
probation period.

**Proposed fix:** If parity is intended, model the pending-creation counter and minimum-invite
gate (and the NEW rank + `clan_newer_time` auto-promotion). If intentionally simplified,
document it as a deliberate behavioral change.

---

## FINDING 8 — Kick: v3 is STRICTER than original (Shaman+ required; chieftain protected)
**Severity: LOW** · **Classification: UNSURE (likely intentional hardening)**

**Original** `_client_clanmember_removereq` (`handle_bnet.cpp:5159-5207`) performs **no rank
check**: any clan member who supplies a target username can remove that member —
including removing the Chieftain. (This is a known legacy weakness.)

**v3** `Clan::kick_member` (`clan.hpp:148-156`) requires the kicker to be Shaman-or-above
(`kit->rank > ClanRank::Shaman → InsufficientRank`) and refuses to kick the Chieftain
(`CannotKickChieftain`).

**Divergence:** v3 enforces authority the original lacks. This is a *behavioral* difference;
clients expecting the lax legacy behavior would now get rejections. Flagged as UNSURE since it
is almost certainly a deliberate security improvement rather than a regression. The authority
comparison itself is internally correct *given* the inverted enum (Finding 1).

**Proposed fix:** None if intentional; just confirm and document. If strict parity is required,
drop the rank check.

---

## What MATCHES (verified correct)

- **Wire rank constants** in `clan_wire_types.hpp:49-53` exactly match the legacy header
  (`kRankNew=0x00, kRankPeon=0x01, kRankGrunt=0x02, kRankShaman=0x03, kRankChieftain=0x04`).
  The bug is only that the *domain* enum (Finding 1) doesn't use these.
- **Clan tag max length = 4** matches `MAX_CLANTAG_LEN`/the 4-byte `t_tag`.
- **Invite authority = Shaman+**: v3 `invite_member` (`clan.hpp:208-209`,
  `rank > Shaman → InsufficientRank`) matches original `clanmember_get_status(member) >= CLAN_SHAMAN`
  (`handle_bnet.cpp:5273, 5371`).
- **MOTD/set-motd authority = Shaman+**: v3 `set_motd` (`clan.hpp:179`) requires Shaman+.
  (Original's MOTD-change handler `clan_save_motd_chg`, `clan.cpp:418-438`, does not re-check
  rank, but the broader convention is Shaman+; v3 is at least as strict.)
- **Disband authority = Chieftain only**: v3 `disband_clan.cpp:24` + `Clan::is_chieftain`
  (`clan.hpp:189`) matches original `clanmember_get_status(member) >= CLAN_CHIEFTAIN`
  (`handle_bnet.cpp:4908`).
- **Leave: Chieftain must disband rather than leave**: v3 `Clan::leave` (`clan.hpp:236`).
  Consistent with the original's separation of disband vs leave (no plain "chieftain leaves"
  path in legacy).
- **New invitee starts as Peon** (`join`/`invite_member` default `ClanRank::Peon`) matches the
  original's `clan_add_member(..., CLAN_PEON)` when `clan_newer_time == 0`
  (`handle_bnet.cpp:5302, 5002`). (When `clan_newer_time > 0` the original uses `CLAN_NEW`
  first — see Finding 7.)

---

## Summary of severities
- HIGH: Finding 1 (inverted rank enum / no wire mapping), Finding 6 (Shaman promote authority + missing Chieftain bound)
- MEDIUM: Finding 2 (max members 250 vs 50), Finding 3 (MOTD 256 vs 223), Finding 4 (name 25 vs 23), Finding 5 (tag min 2 vs 1), Finding 7 (missing creation-pending/probation)
- LOW: Finding 8 (kick stricter than original — likely intentional)
