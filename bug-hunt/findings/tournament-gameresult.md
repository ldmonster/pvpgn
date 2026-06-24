# Bug Hunt — anongame game-result reporting + tournament

Subsystem comparison: ORIGINAL PvPGN (`/home/cnupt/work/pvpgn-server`) vs v3 rewrite (`/home/cnupt/work/pvpgn`).

Scope: result tallying / win-loss-disconnect attribution, anti-cheat result agreement, disconnect
penalty, tournament reply fields, rating/score update on result.

---

## Summary table

| # | Severity | Class | Title |
|---|----------|-------|-------|
| 1 | CRITICAL | BUG / NOT-IMPLEMENTED | No cross-player result agreement / anti-cheat in v3 — single reporter is trusted verbatim |
| 2 | HIGH | BUG | `ReportGameResult::execute` never computes Elo or updates the ladder (header promises it; `ladder_` is dead) |
| 3 | HIGH | BUG | Disconnect-default for missing/absent players is lost in v3 (original defaults non-winners to LOSS/DISC) |
| 4 | MEDIUM | NOT-IMPLEMENTED | Tournament model is a different beast (single-elim bracket) — prelim score / signup-phase tournament not implemented |
| 5 | LOW | UNSURE | Tournament reply `wins/losses/ties` source not wired in v3 use-case (reply builder is pure, but nothing feeds real per-account tournament stats) |
| — | MATCH | — | Tournament reply wire layout, phase/type selection, `convert_time`, type-5 fields, disconnect-is-loss rule, gameresult packet parser |

---

## Finding 1 — No cross-player result agreement / anti-cheat (CRITICAL, BUG/NOT-IMPLEMENTED)

**Original ref:** `src/bnetd/anongame.cpp:1124-1169` (`anongame_evaluate_results`) and `:1190-1227`
(`anongame_stats`).

The original only credits results after **all** w3route connections have closed
(`anongame.cpp:1184-1188`), then runs `anongame_evaluate_results`, which tallies every player's
reported view of every *other* player and overrides each player's self-report by majority:

```c
// anongame.cpp:1144-1163
if (result == W3_GAMERESULT_WIN) wins[number]++;
if (result == W3_GAMERESULT_LOSS) losses[number]++;
...
if (wins[i] > losses[i]) anoninfo->result[i] = W3_GAMERESULT_WIN;   // "others agree on WIN"
else                     anoninfo->result[i] = W3_GAMERESULT_LOSS;  // "others agree on LOSS"
```

It then runs explicit hack-prevention sanity checks (`anongame.cpp:1202-1227`):
- SMALL_FFA must have exactly 1 win, else discard (`return -1`),
- TEAM_FFA must have 2 wins (absent disconnects),
- general games reject `wins > losses`,
- games with `!wins` are discarded entirely (also guards the "host disconnected so everyone
  reports disc" case).

**v3 ref:** `src/application/game/src/report_game_result.cpp:36-69` → `domain/gameplay/game.hpp:140-148`
(`Game::finalize`).

v3 takes the result list from a **single reporter** (`req.reporter_id`, `req.results`), maps it to
`PlayerResult` and calls `Game::finalize`, which stores the list verbatim and emits a `GameEnded`
event. There is:
- no requirement that all players report,
- no cross-player reconciliation/majority vote,
- no per-gametype sanity check,
- no "discard if no winner" guard.

```cpp
// game.hpp:140-148 — finalize just trusts the input
EndOutcome finalize(std::vector<PlayerResult> results, core::SystemTime now) {
    ...
    state_ = GameState::Finalized;
    MatchReport report{id_, client_, std::move(results), now};
    events_.push_back(events::GameEnded{id_, std::move(report)});
    return EndOutcome::Finalized;
}
```

**Divergence:** A single client can declare itself the winner and everyone else losers; v3 will
accept and (once wired) credit it. The original's whole anti-cheat premise — *N independent reports
must agree before a win/loss is credited* — is gone.

**Proposed fix:** Re-introduce a reconciliation step. Accumulate the per-player reports keyed by
reporter, gate finalization on "all live players reported (or their route conn closed)", then run a
majority/agreement pass equivalent to `anongame_evaluate_results` plus the per-gametype sanity
checks and the `!wins` discard, before emitting `GameEnded`. At minimum, do not finalize on a single
report.

---

## Finding 2 — Elo / ladder update never happens in the use-case (HIGH, BUG)

**v3 ref:** `src/application/game/include/application/game/report_game_result.hpp:5-13` documents:

```
///   3. Computes Elo delta for each player via LadderCalculator
///   4. Saves updated LadderEntry for each player
```

and the ctor injects `std::shared_ptr<domain::ladder::ILadderRepository> ladder_`
(`report_game_result.hpp:53,62`).

But `src/application/game/src/report_game_result.cpp:10-70` **never references `ladder_`**, never
constructs a `LadderCalculator`, and never calls `LadderCalculator::compute`
(`domain/ladder/ladder.hpp:56-93`). It only finalizes the game, saves it, and publishes events. No
subscriber re-applies the result to the ladder either — `report_game_result_test.cpp` asserts the
happy path "finalizes successfully" but never checks a rating/ladder change.

**Original ref:** `src/bnetd/anongame.cpp:1290-1342` applies the result to ladder/team stats
(`account_set_saveladderstats(... game_result_win/loss ...)`, `team_set_saveladderstats`) and then
`ladders.update()` (`anongame.cpp:1342`).

**Divergence:** In v3 reporting a game result has **no effect on any player's rating or W/L record**.
The `LadderCalculator` exists and is correct in isolation but is dead code from this path.

**Proposed fix:** Wire `LadderCalculator` into `execute()` (or a `GameEnded` subscriber): load each
participant's `LadderEntry` via `ladder_`, `compute(report, entries)`, apply deltas, and
`ladder_->save(...)`. Match the original's per-gametype handling (solo vs team vs FFA ladders).

---

## Finding 3 — Disconnect/absent-player default lost (HIGH, BUG)

**Original ref:** `src/bnetd/anongame.cpp:1358` (`temp->result[i] = -1; /* consider DISC default */`)
and `:1294-1297`:

```c
int result = a->info->result[i];
if (result == -1) result = W3_GAMERESULT_LOSS;   // never reported => credited a LOSS
```

Every slot starts as DISC (`-1`); a player who never sends a result (disconnected/crashed) is
defaulted to LOSS at credit time, while disconnected players are *skipped* for loading/ack
(`anongame.cpp:1806`, `:1896-1906`) but still penalized in the final tally. The evaluate pass
(Finding 1) also rescues players whose self-report was DISC/LOSS but whom others agree won.

**v3 ref:** `report_game_result.cpp:38-49`. v3 only iterates `req.results` — players the *reporter*
chose to include. There is no per-game roster seeded to "Disconnect by default", so a player omitted
from the reporter's list simply receives **no result at all** (neither loss nor disconnect). The
mapping itself is fine for present entries (`disconnected → Disconnect`, `won → Win`, else `Loss`),
but absence is silently dropped rather than defaulting to a disconnect/loss.

**Divergence:** A disconnecting player who is omitted from the (single) report escapes the loss the
original guarantees. Combined with Finding 1, the reporter fully controls who is penalized.

**Proposed fix:** Seed results from the game's known roster (`game.players_`), default each to
`Disconnect`, then overlay reported outcomes — mirroring the original `result[i] == -1 → LOSS`
fallback and DISC-default initialization.

---

## Finding 4 — Tournament model diverges (single-elim bracket vs prelim-score) (MEDIUM, NOT-IMPLEMENTED)

**Original ref:** `src/bnetd/tournament.cpp`. The original tournament is a **timed, score-based
preliminary tournament** loaded from config: phases driven by `start_preliminary`, `end_signup`,
`end_preliminary`, `start_round_1` (`tournament.cpp:404-486`), per-user stats `wins/losses/ties`
(`tournament.cpp:151-183`), and prelim score `wins*3 + ties - losses` (`tournament.cpp:185-198`).
Game results feed it via `tournament_add_stat(acc, 1|2)` from `anongame_stats`
(`anongame.cpp:1302-1308`), with a `FIXME-TY: how to do ties?` note (ties never recorded by the
original either).

**v3 ref:** `src/domain/matchmaking/include/domain/matchmaking/tournament.hpp:22-60`. v3's
`Tournament` is a **single-elimination bracket** (`schedule(...)`, participants, `start_at`,
`TournamentScheduled` event). It has no signup/prelim phases, no `wins*3+ties-losses` score, no
`tournament_add_stat` equivalent, and no link from game results to tournament standings.

**Divergence:** This is a deliberate redesign, not a like-for-like port. It is flagged because the
*wire protocol still speaks the old prelim-tournament language* (Finding 5 / the reply builder),
so the two halves are inconsistent: the client UI expects prelim phases & W/L/T counts that the new
domain model does not produce.

**Proposed fix:** Decide intent. If the prelim tournament is meant to be preserved, the v3
`Tournament` aggregate does not back the `tournament_reply` path and needs the phase/stat model.
If the bracket model is intended, the reply builder (which faithfully reproduces the legacy
prelim packet) has no real data source. Document the decision; right now it reads as half-ported.

---

## Finding 5 — Tournament reply stat inputs not wired (LOW, UNSURE)

**v3 ref:** `src/application/tournament/src/tournament_reply.cpp` is a **pure function** over
`TournamentInputs` (`tournament_reply.hpp:22-42`), which carries `wins/losses/ties`,
`signed_up`, `in_finals`, `game_in_progress`, `client_supported`, and the phase timestamps. The
builder is faithful (see MATCHES). What is unclear is the **caller**: nothing in
`src/application/tournament` or the matchmaking domain populates `wins/losses/ties` from real
per-account tournament stats (the original pulls them from `tournament_get_stat(account, 1|2|3)` at
`handle_anongame.cpp:877-879,893-895,909-911,925-927`).

**Divergence:** Likely the reply will always carry zero W/L/T unless a caller computes them — but I
could not locate the caller in this subsystem, so marked UNSURE. Worth confirming the handler that
builds `TournamentInputs` actually fills the stat fields (and `signed_up`/`in_finals`).

**Proposed fix:** Confirm/implement the handler that constructs `TournamentInputs` from account
tournament stats; otherwise the reply regresses to zeros vs the original.

---

## What MATCHES (verified correct)

- **Tournament reply phase/type selection** — v3 `tournament_reply.cpp:33-120` reproduces the
  original `handle_anongame.cpp:836-931` branch ladder exactly: type 0 (no tournament, same triple
  condition), type 1 (`start_prelim >= now`), type 2 (`end_signup >= now`), type 3
  (`end_prelim >= now`), type 4 (`start_r1 >= now && game_in_progress`), type 5 (`!in_finals`).
  Types 6/7 are dead in the original (`else if ((0))`) and v3 correctly folds them into type-5
  semantics (`tournament_reply.cpp:112-120`).
- **`convert_time`** — `tournament_reply.cpp:8-15` is byte-identical to
  `_tournament_time_convert` (`handle_anongame.cpp:981-992`): `-1059179400`, `*0.59604645`,
  `+3276999960`.
- **Per-type constant fields** — `unknown4` (0x0000 vs 0x0828), `unknown5` (0x01), `unknown3`
  (0x00 / 0x08 / 0x04), `selection` (2) all match the original per branch, incl. type-5 leaving
  timestamp/countdown/unknown4/unknown5 zero (`tournament_reply.cpp:101-110` vs
  `handle_anongame.cpp:917-931`).
- **Wire struct layout** — `protocol/bnet/anongame.hpp:268-285` field order/sizes match
  `common/anongame_protocol.h:367-390` (`count` u32, `type` u8, `unknown1`/`unknown` u8, `unknown4`
  u16, `timestamp` u32, `unknown5` u8, `countdown` u16, `unknown2` u16, wins/losses/ties u8,
  `unknown3` u8, `selection` u8, `descnum` u8, `nulltag` u8). The legacy `option=7` byte is the
  envelope `sub_option` (`anongame_server.cpp:262-278` encoder + `anongame.cpp:182`
  `kAnonGameClientTournament`).
- **Disconnect-is-loss rating rule** — `domain/ladder/ladder.hpp:44-45,82-88`
  (`disconnect_is_loss = true`, disconnect → loss + rating step at score 0.0) matches the original's
  `result == -1 → W3_GAMERESULT_LOSS` intent (`anongame.cpp:1294-1297`). (The bug is that this code
  is never *invoked* from the report path — Finding 2 — not that the rule is wrong.)
- **Gameresult packet parser** — `anongame_gameresult.cpp` is purely the wire parser
  (number_of_results, per-player number/result/race, part2/part3 stat blocks, heroes). No tally
  logic lives here; the v3 protocol layer mirrors the same fields. No bug in the parser itself.

---

## Notes / leads for follow-up

- `report_game_result.hpp:28-32` `PlayerResult` has `won`/`disconnected` booleans; there is **no
  "tie/draw"** input, yet `MatchOutcome::Draw` exists in the ladder. Draws are unreachable from this
  use-case. The original also never records ties for anongame (`FIXME-TY`), so this is consistent
  for anongame but blocks any future tie support.
- Original `anongame.cpp:1286` divides by `(_count * (tt - 1))` for opponent-level averaging with
  **no zero guard** (the guard at `:1278` only protects `_count` itself). Pre-existing original
  latent div-by-zero for degenerate `tt`/`_count`; not a v3 regression, noted for completeness.
- v3 `report_game_result.cpp:32` calls `game.begin_report()` then `:52` `game.finalize()`. `finalize`
  also accepts `InProgress`, so the `begin_report` transition is somewhat redundant but harmless.
