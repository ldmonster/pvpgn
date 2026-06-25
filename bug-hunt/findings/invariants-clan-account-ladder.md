# Domain Invariants — Clan / Account / Ladder (ORIGINAL vs v3)

Subsystem: aggregate invariants — does v3 enforce the SAME rules the original enforced?

ORIGINAL (read-only): `/home/cnupt/work/pvpgn-server`
v3:                    `/home/cnupt/work/pvpgn`

Legend: severity (state-corrupting invariant violation = HIGH); classification = BUG / INTENTIONAL / UNSURE / NOT-IMPLEMENTED.

Scope note: this file covers *invariants* only. Rating-math parity (init 1000 vs 1500,
ELO formula, K-factor) is already covered in `ladder.md` (F1/F2) and is NOT repeated here,
except where it intersects an invariant. NB: `ladder.md` F1 appears partially stale — the
current `LadderEntry::rating` default is **1000** (`domain/ladder/ladder.hpp:27`), not 1500.

---

## SUMMARY OF FINDINGS

| ID | Invariant | Severity | Class | Match? |
|----|-----------|----------|-------|--------|
| C1 | Exactly one Chieftain | HIGH | BUG | NO |
| C2 | A member belongs to at most one clan | HIGH | NOT-IMPLEMENTED | NO |
| C3 | Promotion bounds (PEON..SHAMAN; can't make a Chieftain) | HIGH | BUG | NO |
| C4 | Shaman may promote subordinates | MED | BUG (stricter) | NO |
| C5 | Rank encoding inverted (rehydrate/wire risk) | MED | UNSURE | partial |
| C6 | Chieftain must disband, not leave | — | — | MATCH |
| C7 | Kick: Shaman+ only, can't kick Chieftain | LOW | — | MATCH (v3 stricter, safe) |
| L1 | Rating floor (never below 1 / never negative) | HIGH | BUG | NO |
| L2 | Wins/losses/disconnects monotonic | — | — | MATCH (domain) / N/A (not wired) |
| L3 | Ladder delta never applied (no record-match wiring) | HIGH | NOT-IMPLEMENTED | NO |
| A1 | Banned-but-expired state consistency | — | — | MATCH |
| A2 | must-change-password / change interaction | — | — | MATCH |
| A3 | Command-group bitmask bounds | — | — | MATCH |

---

# CLAN

## C1 — v3 can reach 0 or 2 Chieftains; "exactly one Chieftain" not preserved  [HIGH, BUG]

**Invariant (original).** A created clan always has exactly one member at
`CLAN_CHIEFTAIN`. The original guarantees this three ways:

1. Rank-update (`CLANMEMBER_RANKUPDATE_REQ`) refuses to set CHIEFTAIN at all —
   `handle_bnet.cpp:5133-5138`:
   ```cpp
   if ((status < CLAN_PEON) || (status > CLAN_SHAMAN)) {
       /* PELISH: CLAN_NEW can not be promoted to anything
        * and also noone can be promoted to CLAN_CHIEFTAIN */
       ... SERVER_CLANMEMBER_RANKUPDATE_FAILED
   }
   ```
   So you can never create a 2nd chieftain via promotion.
2. Chieftain transfer is the *only* way to move the crown, and it is **atomic** —
   `handle_bnet.cpp:5228`:
   ```cpp
   (clanmember_set_status(oldmember, CLAN_GRUNT) == 0) &&
   (clanmember_set_status(newmember, CLAN_CHIEFTAIN) == 0)
   ```
   old chieftain demoted to GRUNT *and* new one promoted in one operation → count stays 1.
3. The chieftain cannot just `leave`; he must `disband` (which deletes the clan),
   so a clan never loses its only chieftain.

**v3 ref** — `src/domain/social/clan.hpp:105-133` and
`src/application/social/src/promote_clan_member.cpp:32-36`.

`Clan::set_rank` / `Clan::promote_member` accept **any** `ClanRank`, including
`Chieftain`, with **no clamp and no atomic demote**:
```cpp
// promote_clan_member.cpp
} else if (new_rank == "chieftain") {
    rank = domain::social::ClanRank::Chieftain;   // accepted!
}
...
clan.promote_member(promoter, target, rank);      // just does set_rank()
```
```cpp
// clan.hpp
PromoteOutcome promote_member(AccountId promoter, AccountId target, ClanRank new_rank) {
    auto pit = find_const_(promoter);
    if (pit == members_.end() || pit->rank != ClanRank::Chieftain)
        return PromoteOutcome::NotAuthorized;
    if (!set_rank(target, new_rank)) return PromoteOutcome::TargetNotMember; // no rank clamp
    return PromoteOutcome::Promoted;
}
```

**Divergence + illegal-state sequences.**
- **TWO chieftains:** Chieftain `A` calls `PromoteClanMember(clan, A, B, "chieftain")`.
  `B` becomes Chieftain while `A` remains Chieftain → **2 chieftains**. The original
  rejects this outright (status > CLAN_SHAMAN failure).
- **ZERO chieftains:** Chieftain `A` promotes `B` to chieftain (now 2), then `A` is
  later kicked. v3's `kick_member` blocks kicking *a* Chieftain, but with two
  chieftains the kicker (a Shaman) can kick neither; however `A` could instead be
  removed via the raw `Clan::remove()` path used by disband, or `B` (now also
  chieftain) leaves via `leave()` → `ChieftainMustDisband` blocks it, but if `A`
  is first demoted by `B` (`promote_member(B, A, "peon")`) then `A` leaves, you can
  reach **0 chieftains** holding only Peons/Grunts. The aggregate never re-asserts
  "≥1 chieftain" after a removal/leave.
- Note `leave()` correctly blocks the sole chieftain from leaving (C6), but that
  guard is meaningless once C1/C3 let a second chieftain exist or a chieftain be
  demoted.

**Proposed fix.**
1. In `promote_member`/`set_rank`, reject `new_rank == Chieftain` (mirror the
   original PEON..SHAMAN clamp). Make crown transfer a dedicated aggregate method
   `transfer_chieftain(current, target)` that atomically demotes the current
   chieftain (to Grunt) and promotes the target — exactly like `handle_bnet.cpp:5228`.
2. In `promote_clan_member.cpp`, drop `"chieftain"` from the accepted rank strings
   (route crown transfer through the dedicated use-case instead).
3. Add an aggregate invariant check (assert/return-error) that `count_of(Chieftain) == 1`
   after every mutating command on a created clan.

---

## C2 — "A member belongs to at most one clan" is unenforceable in v3  [HIGH, NOT-IMPLEMENTED]

**Invariant (original).** A given account can be in at most one clan. The original
enforces this at the invite path — `command.cpp:723-724`:
```cpp
if ((dest_account = accountlist_find_account(username)) && ...
    && (account_get_clan(dest_account) == NULL)
    && (account_get_creating_clan(dest_account) == NULL))
```
i.e. you can only be invited if you currently have *no* clan and aren't mid-creation.
`account_get_clan` is a global per-account back-pointer maintained by
`account_set_clanmember`.

**v3 ref** — `src/application/social/src/join_clan.cpp:22-30`,
`src/application/social/src/invite_to_clan.cpp:24-35`, and the port
`src/domain/social/ports.hpp:36-52`.

`Clan::join` / `invite_member` only check membership of **this** clan
(`if (contains(a)) return AlreadyMember;`). There is no global account→clan index:
`IClanRepository` exposes `find_by_id`, `find_by_tag`, `find_by_name`, `save`,
`remove` — but **no `find_by_account` / `clan_of(account)`**. So no layer can detect
that the invitee is already a member of a *different* clan.

**Divergence + illegal-state sequence.**
- `CreateClan` → clan A with founder X.
- `InviteToClan(clanB, inviterB, X)` (or `JoinClan(clanB, X)`): clan B's aggregate
  sees `contains(X) == false`, admits X. **X is now in two clans.** The original
  rejects this (`account_get_clan(X) != NULL`).
- Knock-on: the `account → clanmember` back-pointer (used for "which clan am I in?")
  becomes ambiguous; disbanding one clan leaves a dangling membership in the other.

**Proposed fix.** Add `IClanRepository::find_by_account(AccountId)` (or a dedicated
membership index) and consult it in `JoinClan`/`InviteToClan` before admitting,
failing with `AlreadyInAnotherClan`. Best modeled as a domain-service / repository
uniqueness guard since the invariant spans aggregates.

---

## C3 — Promotion has no upper bound on rank; can mint a Chieftain  [HIGH, BUG]

(Tightly related to C1; called out separately because the *mechanism* is the missing
rank clamp, independent of the "two chieftains" symptom.)

**Original ref** — `handle_bnet.cpp:5133` clamps the requested status to
`CLAN_PEON..CLAN_SHAMAN`. A Chieftain may set any target to PEON/GRUNT/SHAMAN, never
CHIEFTAIN.

**v3 ref** — `clan.hpp:123-133` (`promote_member`) does no range check on `new_rank`;
`set_rank` (`clan.hpp:105-110`) writes whatever rank it is given.

**Divergence.** v3 allows a Chieftain to set a target's rank to `Chieftain`
(see C1) and offers no equivalent of "can't promote above your own authority".

**Proposed fix.** Clamp `new_rank` to `{Peon, Grunt, Shaman}` in `promote_member`;
return `InvalidRank` otherwise.

---

## C4 — v3 forbids Shamans from promoting; original allows it  [MEDIUM, BUG (stricter divergence)]

**Original ref** — `handle_bnet.cpp:5139-5141`:
```cpp
((clanmember_get_status(member) == CLAN_SHAMAN) && (status < CLAN_SHAMAN)
   && (clanmember_get_status(dest_member) < CLAN_SHAMAN))
 || (clanmember_get_status(member) == CLAN_CHIEFTAIN)
```
A **Shaman** may change ranks of sub-Shaman members (PEON↔GRUNT), as long as the
target is below Shaman and the new status is below Shaman. A Chieftain may do anything
in PEON..SHAMAN.

**v3 ref** — `clan.hpp:125-128`: `promote_member` returns `NotAuthorized` unless the
promoter is **exactly** Chieftain. Shamans cannot promote/demote at all.

**Divergence.** v3 is *stricter* than the original (a behavioral regression, not a
state-corruption). Shamans lose the ability to manage Peons/Grunts. Not state-corrupting,
so MEDIUM.

**Proposed fix.** Mirror the original: allow `promoter.rank == Shaman` when both the
target rank and the new rank are strictly below Shaman (Grunt/Peon).

---

## C5 — Clan rank encoding is INVERTED vs original (rehydrate / wire risk)  [MEDIUM, UNSURE]

**Original** (`clan.h:98-102`): `CHIEFTAIN=0x04, SHAMAN=0x03, GRUNT=0x02, PEON=0x01,
NEW=0x00` — **higher number = higher authority**.

**v3** (`clan.hpp:28-33`): `Chieftain=1, Shaman=2, Grunt=3, Peon=4` —
**lower number = higher authority** (inverted).

The *authority comparisons inside the aggregate are internally consistent* (e.g.
`rank > ClanRank::Shaman` correctly means "below Shaman"), so logic that stays inside
v3 is fine. But two hazards exist:

1. **Persistence/rehydrate parity.** `Clan::rehydrate` (`clan.hpp:69-74`) takes
   `std::vector<ClanMember>` with raw `ClanRank`. If any repository or migration reads
   the legacy on-disk/SQL `status` byte (0..4 with the original meaning) and assigns it
   directly to `ClanRank`, ranks invert: a stored `0x04` (Chieftain) becomes
   `ClanRank(4)` = **Peon**. Verify the persistence mapping flips the encoding.
2. **Wire codec.** `clan_rank_wire.hpp` exists in the tree — confirm it maps
   v3 ↔ legacy status bytes (and that `CLAN_NEW=0`, which v3 has no enum value for,
   is handled — v3 starts the lowest rank at Peon=4 and has no "New/probation" rank;
   the original has `CLAN_NEW=0` for not-yet-accepted invitees, see `command.cpp:727`).

**Proposed fix.** Add a static assertion / explicit conversion table between
`ClanRank` and the legacy status byte at every persistence and wire boundary, and
decide how `CLAN_NEW` (probationary invitee) maps — v3 currently collapses NEW and
PEON, losing the `clan_newer_time` auto-promotion semantics (`clan.cpp:1009-1011`).

---

## C6 — Chieftain must disband, not leave  [MATCH]

Original: chieftain has no "quit"; only `disband` (deletes clan). v3:
`Clan::leave` (`clan.hpp:225-231`) returns `ChieftainMustDisband` for a Chieftain.
**Matches.** (Effectiveness depends on C1/C3 being fixed so a clan can't reach 0/2
chieftains by other paths.)

---

## C7 — Kick authority: Shaman+ only, cannot kick the Chieftain  [MATCH, v3 stricter-safe]

Original: text/packet kick gated by `clanmember_get_status(member) >= CLAN_SHAMAN`
(`command.cpp:677`). The packet-level member-remove handler (`handle_bnet.cpp:5182`)
has **no** explicit "can't remove chieftain" guard — it relies on the client never
sending it. v3 `Clan::kick_member` (`clan.hpp:147-156`) requires Shaman+ AND explicitly
returns `CannotKickChieftain`. v3 is **stricter and safer**; rank semantics match
(the inverted encoding is handled correctly via `rank > Shaman`). **Matches.**

Min-member / auto-disband note: the original does NOT disband a *created* clan when it
shrinks. The only auto-removal is the 120-second pre-creation timeout for clans that
never reached `clan_min_invites` (`clan.cpp:714-716`, `created <= 0`). v3 has no
`created`/`min_invites` lifecycle at all — a clan is "created" immediately on
`Clan::create`. This is a lifecycle simplification (NOT-IMPLEMENTED, low risk for
state corruption) rather than an invariant violation; flagged for completeness.

---

# LADDER

## L1 — Rating has no floor in v3; original clamps to a minimum of 1  [HIGH, BUG]

**Invariant (original).** Rating never drops below 1 and never goes negative. The
original stores rating as `unsigned int` and clamps on every adjustment —
`account_wrap.cpp:1306-1312`:
```cpp
/* don't allow rating to go below 1 */
unsigned int oldrating = account_get_ladder_rating(account, clienttag, id);
unsigned int newrating;
if (delta < 0 && oldrating <= (unsigned int)-delta)
    newrating = 1;
else
    newrating = oldrating + delta;
```

**v3 ref** — `src/domain/ladder/ladder.hpp:26,35,108-113`. `LadderEntry::rating` and
`LadderDelta::rating_delta` are **signed `std::int32_t`**, and `rating_step_` returns a
raw (possibly large-negative) delta with **no floor**:
```cpp
std::int32_t rating_step_(std::int32_t cur, double opp_mean, double score) const {
    const double delta = rules_.k_factor * (score - expected);
    return static_cast<std::int32_t>(std::lround(delta));   // can be negative; no clamp
}
```
There is no code anywhere that applies a delta to an entry and clamps the result (see
L3) — but even the per-step delta is unbounded below, and the entry type permits
negative ratings.

**Divergence + illegal-state sequence.** A heavily-rated player who loses many games
to far-weaker opponents (large positive `expected`, score 0) accrues large negative
deltas. With the deltas summed onto a signed rating and no floor, the stored rating can
go to 0 and **negative**, which the original makes impossible (clamped to 1). Negative
ratings then sort to the very bottom and break any code that assumes a positive
unsigned rating (e.g. the wire codec writes rating as `uint32_t` —
`codec_ladder.cpp:356` — so a negative `int32` becomes a huge `uint32` on the wire).

**Proposed fix.** When applying a delta to an entry (the missing L3 code), clamp the
result with `new_rating = std::max(1, cur_rating + delta)`. Optionally keep
`LadderEntry::rating` unsigned to match the original's domain and make underflow
impossible by construction.

---

## L2 — Wins / losses / disconnects monotonicity  [MATCH at domain level]

Original: each is an `unsigned int` attribute incremented by `+1`
(`account_wrap.cpp:1114-1124`, `:1156`, `:1240-1250`); never decremented.
v3: `LadderDelta` sets each `*_delta` to 0 or 1 and `LadderEntry` counters are
`uint32_t` (`ladder.hpp:36-39, 72-88`). The compute path only ever produces +1 deltas,
so monotonicity holds **if** an apply path eventually adds them (see L3).

Re the earlier "disconnect double-count" concern: in v3 a `Disconnect` outcome with
`disconnect_is_loss = true` increments **both** `disconnects_delta` AND `losses_delta`
(`ladder.hpp:83-87`). This is **consistent with the original**, which on a disconnect
calls *both* `account_inc_ladder_disconnects` and `account_inc_ladder_losses`
(`account_wrap.cpp` inc helpers are invoked separately per result). So
`games = wins + losses + disconnects` would *double-count* a disconnect in BOTH
implementations — i.e. the original also treats a DC as (loss + disconnect). This is a
deliberate "DC counts as a loss" rule, not a v3 regression. **Matches** — no finding,
but note any "games played = W+L+D" derived stat must use `W + L` (with DC already
folded into L) or `W + L_non_dc + D`, never `W + L + D`, in *either* codebase.

---

## L3 — No record-match wiring: LadderCalculator output is never applied  [HIGH, NOT-IMPLEMENTED]

**Observation.** `LadderCalculator::compute` and `LadderDelta` are referenced ONLY by
the domain header and unit tests — grep across `src` finds no production consumer:
```
grep -rln "LadderDelta|compute(" src --include=*.cpp --include=*.hpp  (non-test)
  → src/domain/ladder/include/domain/ladder/ladder.hpp  (only)
```
The match-result use-case `ReportGameResult`
(`src/application/game/src/report_game_result.cpp`) finalizes the *game* aggregate and
publishes game events but **never touches the ladder**: it does not load
`LadderEntry`s, does not call `LadderCalculator`, does not call
`ILadderRepository::save_entry`. The only writer of `save_entry` is `RecomputeLadder`,
which merely **re-sorts and re-saves existing entries** (`recompute_ladder.cpp`) — it
does not change ratings or W/L/D.

**Original ref.** The original applies results on game end:
`ladder.cpp:212 account_adjust_ladder_rating(...)` plus
`account_inc_ladder_wins/losses/disconnects` (`account_wrap.cpp:2308-2313`), driven
from `game.cpp` game-finalization.

**Divergence.** In v3, playing ladder games produces **no rating/stat changes at all**.
The whole rating algorithm is dead code in production. This is why L1's missing floor
isn't user-visible *yet* — but the moment an apply path is wired (as it must be for
parity), the L1 underflow becomes live.

**Proposed fix.** Wire `ReportGameResult` (or a new `RecordLadderMatch` use-case) to:
load the participants' `LadderEntry`s, call `LadderCalculator::compute`, apply each
delta with the L1 floor clamp and monotonic counter increments, and `save_entry` each.

---

# ACCOUNT

## A1 — Banned-but-expired state consistency  [MATCH]

Question: can an account be "banned" yet have an expired ban? In v3 there is no
separate boolean "banned" flag — ban state is a single `std::optional<Ban>`
(`account.hpp:244`), and "is the account barred?" is computed on demand from the
clock: `is_login_barred` / `login` test `ban_->active_at(now)` (`account.hpp:113-117,
149`, `ban.hpp:29-32`). An expired ban therefore cannot read as "currently banned" —
the state is derived, not stored, so the two cannot disagree. Additionally `login`
proactively clears an expired ban and emits `AccountUnbanned` (`account.hpp:159-163`).
This is *cleaner* than the original (which keeps separate ban attributes) and cannot
reach an inconsistent "banned + expired" state. **Matches / improved.** No finding.

## A2 — must-change-password ↔ password-change interaction  [MATCH]

`change_password` always clears `must_change_password_` and emits
`AccountPasswordRotationCleared` only on the true→false edge (`account.hpp:168-177`).
`require_password_change` / `clear_password_change_requirement` are edge-idempotent
(`account.hpp:182-202`). State rule holds: a successful rotation cannot leave the
"must change" flag set. **Matches.** No finding. (Cross-ref: this is the event you
fixed; the state invariant is sound.)

## A3 — Command-group bitmask bounds  [MATCH]

`CommandGroupMask` (`account.hpp:39-65`) bounds every op to groups 1..8 and silently
ignores out-of-range (grant/revoke/has all guard `group >= 1 && group <= kBits`).
`grant_command_group` additionally rejects out-of-range and is edge-idempotent
(`account.hpp:204-209`). No way to set a bit outside 1..8. **Matches.** No finding.

---

# INVARIANTS THAT MATCH (no action)

- C6 chieftain-must-disband; C7 kick authority (v3 stricter-safe).
- L2 W/L/D monotonic + the DC-as-loss double-count is *identical* in both codebases.
- A1 ban/expiry consistency (v3 derives, can't desync); A2 must-change-password;
  A3 command-group bounds.

# HIGHEST-PRIORITY FIXES

1. **C1+C3** — clamp clan promotion to PEON..SHAMAN and make crown transfer atomic;
   add a `count(Chieftain)==1` aggregate invariant. (state corruption: 0/2 chieftains)
2. **C2** — add an account→clan uniqueness guard (`find_by_account`); a member can
   currently join two clans.
3. **L1+L3** — when the ladder apply path is wired, clamp rating to a floor of 1
   (the original's hard rule); until then the rating algorithm is dead code (L3).
