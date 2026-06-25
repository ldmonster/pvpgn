# Bug Hunt — anongame MATCHMAKING ALGORITHM

ORIGINAL: `/home/cnupt/work/pvpgn-server/src/bnetd/anongame.cpp`, `handle_anongame.cpp`
CURRENT v3: `src/domain/matchmaking/*`, `src/application/anongame_lobby/*`, `src/application/anongame_inforeply/*`, `src/protocol/bnet/src/anongame/*`

## TL;DR

The v3 matchmaking layer is a **partial / not-wired model**. There are two
independent, unconnected v3 artifacts that each implement a *fragment* of the
original `_anongame_match` / `_anongame_queue` logic:

1. `domain::matchmaking::AnonGameQueue` (`anon_game_queue.hpp`) — a pure FIFO
   queue keyed on `(ClientTag, team_size)`, matching `2*team_size` players.
2. `application::anongame_lobby::dispatch_admit` (`lobby.cpp`) — a stateless
   "queue / promote / duplicate / reject" decision keyed on `bracket_size`.

**Neither has any production consumer.** Grep for call sites of `.match(`,
`.enqueue(`, `dispatch_admit(` finds only the definitions and unit tests
(`grep -rn '\.match(\|\.enqueue(\|dispatch_admit(' src` → only `lobby.hpp`/`lobby.cpp`
declarations). The `src/protocol/bnet/src/anongame/*` files are *pure
packet codec* (encode/decode of SEARCH / SEARCH_REPLY etc.) — they contain no
queue, no level comparison, no team assignment.

So the **entire matching algorithm is effectively NOT-IMPLEMENTED in
production**. Of the two skeleton models that exist, both are **simplified and
each is independently wrong** versus the original intent in ways documented
below. The `bracket_size_for_game_type` table (`game_type.hpp`) and the
`GameType` enum numbering are the only pieces that faithfully match the
original.

---

## What MATCHES the original (correct)

- **`bracket_size_for_game_type()`** in
  `src/application/anongame_lobby/include/application/anongame_lobby/game_type.hpp:54-76`
  reproduces `_anongame_totalplayers` (original `anongame.cpp:308-341`) value-for-value:
  1v1→2, 2v2/AT2v2/SmallFFA→4, 3v3/AT3v3/2v2v2/AT2v2v2→6, 4v4/AT4v4/TeamFFA/2v2v2v2→8,
  3v3v3→9, 5v5→10, 6v6/4v4v4/3v3v3v3→12, Tournament→0 (dynamic). Correct.
- **`GameType` enum numbering** (`game_type.hpp:25-44`) matches
  `src/common/anongame_protocol.h:448-466` exactly (k1v1=0 … kAT2v2v2=17,
  kTeamFFA=6, kTournament=9). Correct.
- **`dispatch_admit` duplicate detection** (`lobby.cpp:25-34`) mirrors the
  original's reliance on per-account single-queue membership (original tracks
  it via the `t_anongame` conn struct + `anongame_unqueue`). Correct in spirit.

---

## Findings

### F1 — Queue keys on `team_size` and matches `2*team_size`; breaks all non-2-team game types
- **Severity:** High (design-level correctness)
- **Classification:** SIMPLIFIED (model divergence) — would be a BUG if wired
- **Original ref:** `anongame.cpp:804-907` `_anongame_match` selects
  `_anongame_totalplayers(queue)` players for a given *game-type queue*; total
  players is NOT `2*team_size` for multi-team modes. e.g. `2v2v2`→6 players /
  3 teams, `3v3v3`→9 / 3 teams, `4v4v4`→12 / 3 teams, `2v2v2v2`→8 / 4 teams,
  `SmallFFA`→4 players / 0 teams (FFA). See `_anongame_totalteams`
  (`anongame.cpp:343-375`).
- **v3 ref:** `domain/matchmaking/include/domain/matchmaking/anon_game_queue.hpp:58-78`
  ```cpp
  bool can_match() const noexcept {
      return entries_.size() >= 2u * static_cast<std::size_t>(team_size_);
  }
  std::vector<AccountId> match(GameId game) {
      const std::size_t n = 2u * static_cast<std::size_t>(team_size_);
  ```
- **Divergence:** The `2*team_size` formula hard-codes a **two-team
  assumption**. It cannot express 3-team (Nv NvN) or 4-team modes, nor FFA
  (where teams=0). For `2v2v2` it would wait for `2*team_size` = wrong count
  unless `team_size` is mis-set to a non-"per-team" value, and even then the
  resulting players cannot be split into 3 balanced teams downstream.
- **Proposed fix:** Key the queue on the game-type (or carry an explicit
  `bracket_size` + `team_count`), and match `bracket_size_for_game_type(...)`
  players — reuse the existing `game_type.hpp` table rather than `2*team_size`.

### F2 — No level/rating proximity matching (pure FIFO)
- **Severity:** High
- **Classification:** NOT-IMPLEMENTED / SIMPLIFIED
- **Original ref:** `anongame.cpp:804-907`. The original matches by **ladder
  level proximity**: `level = _anongame_level_by_queue(c, queue)` (solo/ffa/at
  ladder, `anongame.cpp:199-231`), `diff = war3_get_maxleveldiff()`
  (`ladder.cpp:461-463`), and only pairs players whose levels fall within
  `[minlevel,maxlevel]` (`anongame.cpp:822-838`). Players are bucketed by level
  in `matchlists[queue][level]` (`anongame.cpp:68`, `:532-540`).
- **v3 ref:** `anon_game_queue.hpp:65-78` `match()` simply drains the
  `n` **oldest** entries (`entries_[0..n)`) with no level consideration.
  `dispatch_admit` (`lobby.cpp`) ignores `LobbyEntry::skill_level`
  (`lobby.hpp:29`) entirely — it is declared but never read.
- **Divergence:** v3 will pair a level-1 player with a level-50 player; the
  original would refuse that pair unless inside `maxleveldiff`.
- **Proposed fix:** Implement level-bucketed selection. Carry the player level
  in `QueueEntry` / `LobbyEntry` and restrict candidate selection to those
  within the configured `maxleveldiff` of the seed player. `skill_level`
  already exists on `LobbyEntry` for exactly this.

### F3 — No search-radius expansion over wait time
- **Severity:** Medium
- **Classification:** SIMPLIFIED / UNSURE
- **Original ref:** The original scans an expanding `delta` window around the
  seed level, `while (abs(delta) < (diff + 1))` (`anongame.cpp:825`), alternating
  ±delta (`:892-898`) and clamping the group span to keep everyone within
  `diff` of each other (`:837-838`). Note: the *acceptance window* is the fixed
  `war3_get_maxleveldiff()` config value — the original does **not** widen the
  window the longer a single player waits across separate search calls; the
  expansion is the within-one-call delta sweep over the level buckets. (The
  prompt's "widens the gap the longer you wait" describes the conceptual goal;
  in this codebase the gap is the fixed `maxleveldiff`, swept via the delta
  loop each search.)
- **v3 ref:** `anon_game_queue.hpp` has `QueueEntry::enqueued_at`
  (`:24-26`) and threads `now` through `enqueue`, but **no wait-time logic uses
  it** — `match()` ignores timestamps entirely (the doc-comment "queue-wait
  windows overlap" at `:6-8` is aspirational; nothing implements overlap).
- **Divergence:** v3 has the timestamp plumbing but no expansion/relaxation
  algorithm; original has the delta sweep over level buckets.
- **Proposed fix:** When implementing F2, port the delta sweep (`anongame.cpp:825-903`)
  so the acceptance band is centered on the seed and bounded by `maxleveldiff`.

### F4 — No team assignment / balancing
- **Severity:** High
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** `_anongame_order_queue` (`anongame.cpp:556-802`) sorts the
  matched players by level (`std::qsort(..., _anongame_compare_level)`,
  `:877`) then redistributes them across 2/3/4 teams to balance per-team level
  sums (the giant teams==2/3/4 permutation block). `_anongame_compare_level`
  (`:545-554`) orders descending by level.
- **v3 ref:** Neither `AnonGameQueue::match` nor `dispatch_admit` produces any
  team split. `match()` returns a flat `std::vector<AccountId>` in FIFO order;
  `dispatch_admit` returns `promoted_party` in FIFO order
  (`lobby.cpp:45-52`). No sort, no per-team grouping, no balance.
- **Divergence:** Even for the 2-team case the players are not sorted by level
  nor snaked into balanced teams.
- **Proposed fix:** After selection, sort by level descending and apply a
  balancing pass (the original's permutation logic, or a simpler greedy/snake
  draft) to assign the flat list into `team_count` teams.

### F5 — `dispatch_admit` rejects instead of matching when the queue is already full
- **Severity:** Medium
- **Classification:** SIMPLIFIED (behavioral divergence) — would be a BUG if wired
- **Original ref:** Original promotes precisely when the Nth player arrives
  (`anongame.cpp:860` / `:875` `players[queue] == _anongame_totalplayers(queue)`),
  and the queue can never legitimately exceed the bracket because matched
  players are immediately unqueued (`:863-864`, `:881-882`).
- **v3 ref:** `lobby.cpp:38-41`
  ```cpp
  if (req.current_queue.size() + 1u > req.bracket_size) {
      return out;  // kRejected
  }
  ```
  If `current_queue.size() == bracket_size` already (e.g. the caller failed to
  drain after a previous promotion, or two entrants race), this returns
  **kRejected** and silently drops the entrant rather than forming a game.
- **Divergence:** The original treats "enough players" as the *trigger* to
  build a game; v3 treats "queue already at capacity" as an error. This is
  only safe under the strict invariant that the caller drains the queue on
  every `kPromoted`. With AT (arranged-team) parties added atomically, a single
  entrant can also push the size *past* exact-fill (original handles this by
  adding a whole team at once, `:853-857`), and v3 would reject it.
- **Proposed fix:** Treat `>= bracket_size` (after append) as promote, not
  reject; or document/enforce the drain invariant and handle AT party
  group-inserts that can land on `> bracket_size` exactly.

### F6 — AT (arranged-team) group enqueue/match not modeled
- **Severity:** Medium
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** `_anongame_match` has a dedicated AT branch
  (`anongame.cpp:842-868`): for `anongame_arranged(queue)` it inserts an entire
  pre-formed team's connections at once (`a->tc[i]`), interleaving them across
  team slots (`player[queue][teams + i*totalteams]`, `:854`), and unqueues by
  team entry, not by individual player. The AT search also gates on every team
  member having sent a matching `tid` (`handle_anongame.cpp` AT branches,
  `anongame.cpp:478-491`).
- **v3 ref:** `AnonGameQueue` enqueues a single `AccountId` at a time
  (`anon_game_queue.hpp:39-45`); `dispatch_admit` admits a single `entrant`
  (`lobby.cpp:17`). No notion of an atomic team insert or `tid`-based all-members-ready gate.
- **Divergence:** AT matchmaking (AT2v2/AT3v3/AT4v4/AT2v2v2) is entirely
  absent. PG-only model.
- **Proposed fix:** Support group enqueue (insert a vector of accounts as one
  unit) and the all-members-arrived gate before the party becomes matchable.

### F7 — No map-preference (`map_prefs`) intersection during matching
- **Severity:** Medium
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** Players only match if their map-pref bitmasks intersect:
  `(cur_prefs & md->map_prefs)` (`anongame.cpp:834`), with the running
  `cur_prefs &= md->map_prefs` (`:839`) and final map chosen from the
  intersection via `_get_map_from_prefs` (`:866`,`:884`). Also gated on equal
  `versiontag` (`:831-834`).
- **v3 ref:** `QueueEntry` (`anon_game_queue.hpp:23-26`) carries only
  `account` + `enqueued_at`; `LobbyEntry` (`lobby.hpp:25-30`) has no map_prefs
  field. No intersection logic anywhere.
- **Divergence:** v3 could match players who share no acceptable map (and has
  no map-version compatibility check), which the original explicitly prevents.
- **Proposed fix:** Carry `map_prefs` (and version tag) on the queue entry and
  require non-empty intersection as a match precondition; select the map from
  the accumulated intersection.

### F8 — Match commit ("found") gating: original commits on fill, no per-player accept round (informational)
- **Severity:** Low (parity note, not a bug)
- **Classification:** INTENTIONAL / matches
- **Original ref:** After `_anongame_match` fills the bracket, the game is
  committed immediately: `handle_anongame_search` calls `_anongame_search_found`
  the moment `players[queue] == _anongame_totalplayers(queue)`
  (`anongame.cpp:509-511`); `_anongame_search_found` (`:912+`) builds and pushes
  the w3route game-found packet to all participants. There is **no** separate
  "all players clicked accept" gating step in the W3 anon-game flow — fill *is*
  the commit. Cancellation is handled separately (`handle_anongame.cpp:406+`
  `_client_anongame_cancel`).
- **v3 ref:** `AnonGameQueue::match` emits a single `AnonGameMatched` event on
  fill (`anon_game_queue.hpp:76`); `dispatch_admit` returns `kPromoted` on fill
  (`lobby.cpp:45-53`). This **matches** the original's commit-on-fill semantics.
- **Note:** No divergence here. Flagged only because the task asked about the
  "all-accepted gating"; in this codebase that gating does not exist — fill is
  the gate, and v3 preserves that.

### F9 — `domain/matchmaking/events.hpp` is an empty TODO stub
- **Severity:** Low
- **Classification:** NOT-IMPLEMENTED (incomplete scaffolding)
- **v3 ref:** `src/domain/matchmaking/include/domain/matchmaking/events.hpp:8-14`
  is a TODO comment with no event types in the `matchmaking` namespace. The
  events actually used by `AnonGameQueue` (`AnonGameQueued`, `AnonGameDequeued`,
  `AnonGameMatched`) come from `domain/shared/events.hpp` instead.
- **Divergence:** Dead/placeholder file; harmless but indicates the bounded
  context is unfinished. No original counterpart.
- **Proposed fix:** Remove the stub or move the anon-game events here for
  cohesion (non-functional).

---

## Severity summary

| ID | Severity | Class | One-liner |
|----|----------|-------|-----------|
| F1 | High | SIMPLIFIED | `2*team_size` can't express 3/4-team or FFA modes |
| F2 | High | NOT-IMPL | No level/rating proximity; pure FIFO; `skill_level` unused |
| F3 | Medium | SIMPLIFIED | No delta-sweep over level buckets; `enqueued_at` unused |
| F4 | High | NOT-IMPL | No team sort/balance (original's `_anongame_order_queue`) |
| F5 | Medium | SIMPLIFIED | `dispatch_admit` rejects at-capacity instead of promoting |
| F6 | Medium | NOT-IMPL | No AT arranged-team group enqueue / `tid` ready-gate |
| F7 | Medium | NOT-IMPL | No `map_prefs` intersection / version-tag match gate |
| F8 | Low | MATCHES | Commit-on-fill semantics preserved (no separate accept round) |
| F9 | Low | NOT-IMPL | Empty `events.hpp` TODO stub |

## Bottom line

The production v3 anongame path is codec-only; the matching *algorithm* is not
wired in. The two skeleton models (`AnonGameQueue`, `dispatch_admit`) are
FIFO-only and omit the four pillars of the original algorithm — level-proximity
selection (F2/F3), team balancing (F4), AT group matching (F6), and map/version
gating (F7) — plus the `2*team_size` model can't represent multi-team/FFA game
types (F1). The static lookup tables and enum numbering (`game_type.hpp`) are
the only faithfully-ported pieces. No false-positive "implemented-but-subtly-
wrong arithmetic" bugs were found, because the arithmetic that exists (bracket
table, FIFO drain) is internally correct — it just does far less than the
original.
