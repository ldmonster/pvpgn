# Ladder + Rating Calculation — Bug Hunt (ORIGINAL vs v3)

Subsystem: ladder ordering, ELO/rating math, rank assignment, ladder reply encoding.

ORIGINAL (read-only): `/home/cnupt/work/pvpgn-server`
v3: `/home/cnupt/work/pvpgn`

Legend: severity = how wrong the user-visible result is; classification = BUG / INTENTIONAL / UNSURE.

---

## F1 — Wrong initial/default rating: 1500 vs original 1000  [HIGH, BUG]

**Original ref** — `src/common/setup_before.h:227`:
```cpp
const unsigned BNETD_LADDER_INIT_RAT = 1000;
```
used as the default for `ladder_init_rating` (`src/bnetd/prefs.cpp:2299-2302`) and applied on first ladder play (`src/bnetd/ladder.cpp:103`):
```cpp
account_adjust_ladder_rating(account, clienttag, id, prefs_get_ladder_init_rating());
```

**v3 ref** — `src/domain/ladder/include/domain/ladder/ladder.hpp:27, 55, 64, 97, 105`:
```cpp
std::int32_t rating = 1500;                 // default LadderEntry rating
... (i < entries.size()) ? entries[i].rating : 1500;   // missing entry fallback
if (entries.size() <= 1) return 1500.0;     // opponent_mean fallback
```

**Divergence.** PvPGN's classic BNET ladder seeds every new player at **1000**, not the chess-standard 1500. v3 hardcodes 1500 in five places. Every fresh account starts 500 points off, and because the ELO expected-score uses the opponent's rating, the first several matches against unseeded opponents compute the wrong expected score and therefore the wrong delta. Original parity is explicitly claimed in the header doc comment ("Today's `ladder_calc.cpp` is being lifted here piecewise").

**Proposed fix.** Default `rating` to 1000 and source the seed value from a prefs/config port (`prefs_get_ladder_init_rating()` equivalent) rather than hardcoding, so the admin-configurable `ladder_init_rating` is honored. Replace all three 1500 fallbacks accordingly.

---

## F2 — ELO expected-score formula sign/encoding differs; K-factor hardcoded 32 vs original 50/30/20  [HIGH, BUG re: parity claim / INTENTIONAL simplification]

**Original ref** — `src/bnetd/ladder_calc.cpp`:
- probability (`:77-86`): `i = (a-b)/400; Pwin = 1/(1+10^(-i))`.
- K coefficient (`:101-114`):
```cpp
if (total_ladder_games < 30) return 50;
if (rating < 2400) return 30;
return 20;
```
- delta on win (`:615-618`): `delta = fabs(k*(1-prob)/team_members)`, on non-win: `-fabs(k*prob)`.
- final adj is truncated: `info[curr].adj = (int)delta;` (`:624`) — **truncation toward zero**, not rounding.

**v3 ref** — `src/domain/ladder/include/domain/ladder/ladder.hpp:41-46, 108-113`:
```cpp
double k_factor = 32.0;   // "W3 default; SC uses 16"
...
const double expected = 1.0/(1.0+std::pow(10.0,(opp_mean - cur)/400.0));
const double delta = rules_.k_factor*(score - expected);
return static_cast<std::int32_t>(std::lround(delta));
```

**Divergence.**
1. **K-factor:** original uses experience/rating-tiered K (50 / 30 / 20). v3 uses a flat 32 (or 16). For classic SC/BW/W2BN ladders this produces materially different point swings, especially for new players (50 vs 32) and high-rated players (20 vs 32). The comment "SC uses 16" contradicts the original, which never uses 16 anywhere in `ladder_calc.cpp`.
2. **Rounding:** original truncates the magnitude (`(int)delta`, toward zero); v3 uses `std::lround` (round half away from zero). For a delta of e.g. 24.6 original gives 24, v3 gives 25. Off-by-one rating drift accumulates over many games.
3. **Multi-player / team handling:** original divides the win delta by `team_members` and computes `prob` via the full `two_player … eight_player` tournament expansion (`:123-516`). v3 collapses all opponents to a single `opponent_mean_` average rating and a plain 2-player expected score. This is a deliberate simplification (header says "simple Elo-style + W3 K-factor 32 sufficient for tests"), so the *model* choice is INTENTIONAL — but the header also claims it is "lifting `ladder_calc.cpp` piecewise," i.e. parity-intent, and it does NOT match the original for any FFA/team game.

**Proposed fix.** If parity is intended: restore tiered K (games<30 →50, rating<2400 →30, else 20) configurable per client tag; truncate with `(int)` not `lround`; and either implement the N-player expansion or document explicitly that multi-player FFA/team rating is out of scope (not "lifted"). At minimum correct the misleading "SC uses 16 / lifting piecewise" comments.

---

## F3 — Recompute sorts by WINS, original (highestrated) sorts by RATING  [HIGH, BUG]

**Original ref** — `src/bnetd/ladder.cpp`. The primary/"default" account ladder sort is `ladder_sort_highestrated`, whose primary key is **rating** (`LadderReferencedObject::getData`, `:687-691`):
```cpp
case ladder_sort_highestrated:
    primary_ = rating; secondary_ = wins; tertiary_ = ratio; return true;
```
and `account_get_ladder_rank` / `setRank` are only assigned from the highestrated/default ladder (`:730-744, 762-771`). The comparator sorts primary **descending** (`LadderEntry::operator<`, `:940-950`).

**v3 ref** — `src/application/ladder/src/recompute_ladder.cpp:32-38`:
```cpp
// 3. Sort by wins descending (ties broken by rating descending)
std::stable_sort(entries.begin(), entries.end(),
    [](const LadderEntry& a, const LadderEntry& b) {
        if (a.wins != b.wins) return a.wins > b.wins;
        return a.rating > b.rating;
    });
```

**Divergence.** v3's recompute ranks the canonical ladder by **wins** (rating only as tie-break). The original's rank-bearing ladder ranks by **rating** (wins only as tie-break). These produce different #1..#N orderings and therefore different ranks reported to clients. v3 has effectively swapped the primary and secondary sort keys relative to the original's highestrated ladder, which is the one that feeds `account.rank`.

**Proposed fix.** Sort by `rating` descending, tie-break by `wins` descending, then by win-ratio (`(wins<<10)/games`) descending, then uid — matching original `LadderEntry::operator<` and `getData` for `ladder_sort_highestrated`. If v3 deliberately exposes a "most wins" ladder, it should be a separate sort variant, not the default rank source.

---

## F4 — Tie-break chain incomplete vs original (no win-ratio / uid tertiary)  [MEDIUM, BUG]

**Original ref** — `src/bnetd/ladder.cpp:684-700` computes `ratio = (wins<<10)/games` as the **tertiary** key, and `LadderEntry::operator<` (`:940-950`) breaks ties primary→secondary→tertiary(ratio)→uid, all descending, giving a fully deterministic stable total order.

**v3 ref** — `recompute_ladder.cpp:33-38` breaks ties on only two keys (wins, rating) and relies on `std::stable_sort` for the remainder. `get_ladder_entry.cpp` / `get_ladder_page.cpp` don't sort at all — they trust the repository order.

**Divergence.** v3 drops the win-ratio tertiary key and the uid final tie-break. Two players with equal wins+rating get an order that depends on repository iteration order, not the deterministic ratio/uid chain the original guarantees. Ranks can differ from the original and can be non-deterministic across reloads.

**Proposed fix.** Add the tertiary win-ratio key `(wins<<10)/games` and a final `account`/uid tie-break to the comparator, matching original `operator<`.

---

## F5 — No MaxRankKeptInLadder (1000) cap; rank assigned to unbounded list  [MEDIUM, BUG/UNSURE]

**Original ref** — `src/bnetd/ladder.cpp:56` `#define MaxRankKeptInLadder 1000`, enforced in `sortAndUpdate` (`:1000-1017`): entries beyond rank 1000 get rank 0 and are **erased** from the ladder.

**v3 ref** — `recompute_ladder.cpp:21` caps fetch at `kMaxEntries = 100'000`; `get_ladder_page.cpp:32` / `get_ladder_entry.cpp:34` likewise. No 1000-entry cap; ranks are assigned to every entry, and players past 1000 keep a nonzero rank.

**Divergence.** Original ranks only the top 1000 and zeroes/evicts the rest. v3 ranks up to 100k. A client querying rank for a low player gets a large rank in v3 where the original would report 0 ("not ranked"). Behavioral divergence in `getRank`-equivalent paths. UNSURE whether the BNET ladder cap is intended to be preserved in the v3 redesign, but it is original behavior and is not reproduced.

**Proposed fix.** If preserving classic BNET semantics, cap ranked entries at 1000 and report rank 0 beyond that.

---

## F6 — D2 ladder tie-break on equal experience differs from original  [LOW/MEDIUM, BUG]

**Original ref** — `src/d2dbs/d2ladder.cpp` `d2ladder_find_pos` (`:172-191`):
```cpp
i = d2ladder->len;
while (i--) {
    if (d2ladder->info[i].experience > info->experience) {   // strict >
        if (strncasecmp(d2ladder->info[i].charname, info->charname, ...)) i++;
        break;
    }
    if (i <= 0) break;
}
```
Also gated by a threshold: `if (info->experience < prefs_get_ladderupdate_threshold()) return -1;` (`:178`).

**v3 ref** — `src/domain/ladder/src/d2_ladder.cpp:74-81`:
```cpp
auto pos = std::lower_bound(entries_.begin(), entries_.end(), entry,
    [](const D2LadderEntry& a, const D2LadderEntry& b){
        return a.experience > b.experience; });  // descending, strict
entries_.insert(pos, std::move(entry));
```

**Divergence.**
1. **Equal-experience placement:** original walks from the bottom and inserts a new char *after* (lower than) all entries with strictly-greater experience — i.e. on equal experience the newcomer is placed **below** existing equals (stable, incumbents keep higher rank). `std::lower_bound` with `a.experience > b.experience` returns the first position where `entry.experience >= existing.experience`, inserting the newcomer **above** existing equal-experience entries. Tie order is inverted vs the original — a newly-tying character outranks incumbents in v3 but not in the original.
2. **No experience threshold gate:** original refuses to insert characters below `ladderupdate_threshold`; v3 `add()` has no such gate, so sub-threshold characters appear on the ladder. (May be intentional if threshold is enforced upstream — UNSURE — but no gate exists in the domain aggregate that the doc says "mirrors the legacy d2ladder module.")

**Proposed fix.** Insert at `upper_bound` (or otherwise place equal-experience newcomers below incumbents) to match incumbent-priority tie order, and add/forward the experience threshold gate.

---

## What MATCHES (no bug)

- **ELO probability formula core:** `1/(1+10^((opp-cur)/400))` in v3 (`ladder.hpp:109-110`) is algebraically identical to the original `1/(1+10^(-(a-b)/400))` (`ladder_calc.cpp:81-85`). The 400 scaling and base-10 are correct.
- **1-based rank assignment:** both original (`sortAndUpdate` starts `rank = 1`, `ladder.cpp:992`) and v3 (`get_ladder_page.cpp:68` `rank = i + 1u`; `d2_ladder.cpp:49` `return i + 1`; `rank_of`) are 1-based. No off-by-one in rank numbering itself.
- **Sort direction (descending):** v3 recompute and D2 both sort descending (higher first), matching the original's descending `operator<`. The bug is the *key*, not the direction.
- **LADDERREPLY wire encoding:** `codec_ladder.cpp` encode/decode of `LadderListReply` (`:335-371` / `:58-95`) is symmetric and field order (current{wins,loss,disc,rating,rank}, active{...}, ttest[], lastgame u64×2, name) is self-consistent. No field-order or width bug found within the codec itself. (Not cross-checked against the original byte layout in this pass — flagged as not-verified, not as a bug.)
- **Disconnect → loss for rating:** v3 `disconnect_is_loss = true` (`ladder.hpp:45, 82-88`) treats a disconnect as a loss for rating purposes. The original `coefficient`/games count includes disconnects in `total_ladder_games` and the loss branch handles non-win results uniformly, so counting a disconnect against rating is consistent in spirit. (Original tracks disconnects as a *separate* counter too; v3 increments `disconnects_delta` AND `losses_delta` — see note below.)

## Note / UNSURE — disconnect double-count

v3 `compute` on `Disconnect` sets BOTH `disconnects_delta = 1` and `losses_delta = 1` (`ladder.hpp:82-88`). The original keeps wins/losses/disconnects as independent counters (`account_get_ladder_disconnects` is separate from losses; `games = wins+losses+disconnects+draws`, `ladder.cpp:205-207`). If v3's `games` is later derived as `wins+losses+disconnects`, a disconnect is counted **twice** (once as loss, once as disconnect), inflating game count and skewing win-ratio. UNSURE without seeing the v3 games/ratio derivation, but flagged: original does not add a disconnect to the loss counter.

---

## Severity summary
| ID | Severity | Class | One-liner |
|----|----------|-------|-----------|
| F1 | HIGH | BUG | initial rating 1500, should be 1000 |
| F2 | HIGH | BUG/INTENTIONAL | flat K=32 + lround + no N-player expansion vs tiered K=50/30/20 + truncation |
| F3 | HIGH | BUG | recompute ranks by wins; original rank ladder is by rating |
| F4 | MEDIUM | BUG | missing win-ratio + uid tie-break keys |
| F5 | MEDIUM | BUG/UNSURE | no 1000-rank cap/eviction |
| F6 | LOW/MED | BUG | D2 equal-experience tie order inverted; no threshold gate |
</content>
</invoke>
