# Invariants & Lifecycle: Game and Channel aggregates — ORIGINAL vs v3

Scope: domain invariants / state-machine rules for the **Game** and **Channel**
aggregates. Read-only audit. ORIGINAL = `/home/cnupt/work/pvpgn-server`,
v3 = `/home/cnupt/work/pvpgn`.

Big-picture context that colours every finding below:

* In the ORIGINAL, the game "FSM" is **client-driven**. The hosting client
  literally tells the server the status (`open` / `full` / `started` / `done`)
  via `STARTGAME1`/`STARTADVEX` packets, and `game_set_status`
  (game.cpp:1437) is the only gate. The server never autonomously decides a
  game is full from a player count — capacity is enforced *by the client*, and
  the server only mirrors what the client reports. v3 instead models a genuine
  server-side FSM (`Open→InProgress→Reporting→Finalized`) inside the `Game`
  aggregate with server-side capacity. So several "divergences" are really v3
  *adding* an invariant the original delegated to the client. These are flagged
  INTENTIONAL but with caveats.
* The ORIGINAL channel join/membership invariants (max-members, leave-old-
  channel, banned redirect) live in `conn_set_channel` (connection.cpp:1877),
  **not** in `channel_add_connection`. The tempOP and auto-delete rules live in
  `channel_add_connection` / `channel_del_connection` (channel.cpp:430/503).

---

## GAME

### G-MATCH-1 — join rejected when not Open and when full — MATCHES (with caveat)
* original: handle_bnet.cpp:3871-3919 (`_client_gamelistreq`) rejects join when
  status is `started` (SSTATUS_STARTED), `full` (SSTATUS_FULL), `done`
  (SSTATUS_NOTFOUND); only `open`/`loaded` proceed.
* v3: `Game::join` (game.hpp:98-105) returns `Closed` when `state_ != Open` and
  `Full` when `is_full()`. JoinGame use-case maps these to `GameClosed`/`GameFull`
  (join_game.cpp:23-31).
* Verdict: the *open + not-full* rule MATCHES. Caveat: the original also admits
  `game_status_loaded` (a save-game resume state) which v3 has no equivalent of;
  not a bug, just a missing feature (see G-STATE-2).

### G-MATCH-2 — duplicate join (same account twice) — MATCHES
* original: `game_add_player` reuses an empty slot but does not actively reject
  an account that is already present (it is gated upstream by `conn_get_game`
  state). v3 explicitly returns `AlreadyIn` (game.hpp:100) → `AlreadyInGame`.
* Verdict: v3 is *stricter* and correct; no illegal state. MATCH / improvement.

### G-MATCH-3 — host counts as a player — MATCHES
* original: the creating connection is added via `game_add_player` and counts in
  `game->ref`/`count`; `game_choose_host` always picks a current connection.
* v3: `Game::host` pushes the host into `players_` (game.hpp:63) so
  `player_count()` includes the host and `is_full()` counts the host.
* Verdict: MATCH.

### G-MATCH-4 — FSM cannot go backwards / start-twice / finalize-twice — MATCHES
* original: `game_set_status` (game.cpp:1445-1456) forbids `started → open/full/
  loaded` and forbids `done → anything-but-done`.
* v3: `start` requires `state_==Open` else `WrongState` (game.hpp:123); `finalize`
  requires `InProgress|Reporting` else `NotInProgress` (game.hpp:141);
  `begin_report` only advances `InProgress→Reporting` and is idempotent in
  `Reporting`. A second `finalize` from `Finalized` returns `NotInProgress`.
* Verdict: the "no going backwards, no double-finalize, no double-start" family
  MATCHES and is in fact enforced more cleanly in v3.

### G-1 — `Game::host` / start use-cases cap max_players at 16, original is unbounded — LOW — UNSURE/INTENTIONAL
* original: `game_set_maxplayers` (game.cpp:1305) stores whatever the client
  sends; there is no 1..16 clamp in the aggregate. maxplayers is purely
  informational (the client enforces it).
* v3: `Game::host` rejects `max_players==0 || >16` (game.hpp:58-61); StartGame
  (start_game.cpp:16) and CreatePrivateGame (create_private_game.cpp:21) repeat
  the same 1..16 guard.
* Divergence: a game advertised with maxplayers 24 (e.g. some UMS/teamFFA W3
  custom games allow >16 slots in the lobby protocol) would be *accepted* by the
  original but *rejected* by v3 with `MaxPlayersOutOfRange`.
* Illegal/forbidden sequence: original allows `maxplayers=24`; v3 forbids it.
* Severity LOW (rejects a legal-in-original config, does not create an illegal
  state). Classification UNSURE — likely intentional hardening, but the bound
  should be justified against the real client cap. Proposed fix: confirm the
  protocol max (W3 supports up to 24 in some modes) and either raise the cap or
  document why 16 is correct; keep the guard but make it a named constant shared
  by domain + use-cases.

### G-2 — v3 has no `Full` state distinct from `Open`; capacity is implicit — MEDIUM — INTENTIONAL (divergence worth noting)
* original states: `started, full, open, loaded, done` (game.h:75-79). `full` is
  a *distinct, client-reported* status. A game can be `full` (no new joins) yet
  still in the lobby (not started). The GAMELISTREPLY distinguishes
  OPEN vs FULL vs STARTED to the client (handle_bnet.cpp:3789-3805).
* v3 states: `Open, InProgress, Reporting, Finalized` (game.hpp:33-38). There is
  no `Full` state; "full" is derived from `players_.size() >= max_players`.
* Divergence: in v3, a lobby that the host has marked FULL but not yet STARTED is
  still `Open`. If `player_count < max_players` (e.g. a player left after the
  host marked it full), v3 would let a new player `join` even though the original
  host had set status=full to lock the lobby. The original would keep rejecting
  (status==full) until the host re-opens it.
* Example v3 allows that original forbids: host sets game FULL with 7/8 players;
  one leaves (6/8); a stranger joins → v3 `Joined`, original still rejects
  (status==full, SSTATUS_FULL).
* Severity MEDIUM — wrong/illegal "lobby locked" semantics, but only reachable
  once the host-controlled FULL flag exists; today no v3 path ever sets FULL, so
  it is latent. Classification INTENTIONAL simplification, but it *drops a real
  invariant* (host can lock a non-full lobby). Proposed fix: when wiring the real
  STARTADVEX/STARTGAME handlers, add a host-settable `locked/full` flag (or a
  `Full` state) so the host can close the lobby independent of capacity, and have
  `join` reject on it.

### G-3 — `loaded` (save-game resume) state not modelled — LOW — NOT-IMPLEMENTED
* original: `game_status_loaded` is a joinable state (treated like `open` for
  joins, handle_bnet.cpp:3884-3895) used for resumed/loaded games.
* v3: no equivalent. Resumed games cannot be represented.
* Severity LOW (missing feature, not an illegal state). Classification
  NOT-IMPLEMENTED. Proposed fix: add a `Loaded` state joinable like `Open` if/when
  save-game resume is supported.

### G-MATCH-5 — host migration on host-leave — MATCHES (recently fixed)
* original: `game_del_player` (game.cpp:1699) NULLs the leaver's slot then calls
  `game_choose_host` (game.cpp:64-88), which sets `game->owner` to the **first
  remaining non-NULL connection** (earliest-joined survivor). Migration happens
  on *every* leave, and the host is always a current connection.
* v3: `Game::leave` (game.hpp:107-120): if `who == host_ && !players_.empty()`,
  `host_ = players_.front()`. LeaveGame reports `host_migrated`/`new_host`
  (leave_game.cpp:32-41).
* Verdict: MATCH. Both migrate to the earliest remaining player; both keep the
  invariant "host is always a current player." v3 only re-assigns when the host
  itself left (an optimisation — the original re-runs `game_choose_host`
  unconditionally but lands on the same player when the host did not leave).
* Minor note (not a bug): v3 emits a `GamePlayerLeft` event but **no explicit
  host-migration domain event**; the new host is surfaced only through the
  use-case return value. The original likewise has no migration packet beyond the
  refreshed game list. Acceptable, but consider an event for observers.

### G-4 — original reports+destroys the game when the 2nd-to-last player leaves; v3 keeps a 1-player game alive — MEDIUM — BUG (divergence)
* original: `game_del_player` (game.cpp:1745-1752): `if (game->ref < 2)` (i.e.
  after this removal only one or zero players would remain) it calls
  `game_report(game)` then `game_destroy(game)` and returns. So a game is torn
  down as soon as it drops below 2 players.
* v3: LeaveGame (leave_game.cpp:44) only deletes when `player_count()==0`. A
  single remaining player keeps the game (in any state) alive indefinitely.
* Example v3 allows that original forbids: 2-player started game; one leaves →
  original reports the result and destroys the game; v3 leaves a live 1-player
  `InProgress` game with the survivor as host, never finalized.
* Severity MEDIUM — leaves a "zombie" 1-player game (resource leak + a game the
  ladder never receives a result for). Not an *illegal* aggregate state per se,
  but it diverges from the original lifecycle invariant "a match needs ≥2
  players to continue." Classification BUG (lifecycle divergence). Proposed fix:
  in LeaveGame, when the game is `InProgress`/`Reporting` and `player_count()`
  drops below 2, finalize/report and delete it (mirror `ref < 2`). For lobby
  (`Open`) games the original keeps a 1-player open lobby alive, so gate the
  auto-finalize on state==InProgress.

### G-5 — `report_game_result` lets ANY player (incl. non-host) drive finalize — LOW — INTENTIONAL/UNSURE
* original: results are reported per-connection and the game is finalized when
  all leave / report; there is no single "finalizer" authority — `game_report`
  fires on teardown.
* v3: ReportGameResult (report_game_result.cpp:27-55) only checks the reporter is
  *in* the game, then `begin_report()` + `finalize()` immediately on the first
  report, finalizing for everyone.
* Divergence: in v3 the first player to report instantly finalizes the whole
  match with that one report's results; a later/peer report is then rejected
  (`NotInProgress`). The original aggregates reports across players before the
  final tally. This is a results-integrity difference more than a state-machine
  illegality.
* Severity LOW (no illegal aggregate state; affects result correctness/trust).
  Classification UNSURE — may be an intentional simplification. Proposed fix:
  if multi-party result reconciliation matters, accumulate per-player reports in
  `Reporting` and only `finalize` once a quorum/all-present condition is met.

### G-6 — `rehydrate` can resurrect an illegal aggregate (empty players, or host not in players) — LOW — BUG (defensive)
* v3: `Game::rehydrate` (game.hpp:71-78) sets state/players verbatim with no
  validation. A persisted row with `players=[]` or with `host` not in `players`
  would rebuild an aggregate that violates "host is always a current player" and
  "non-empty while alive."
* original: reconstruction goes through `game_add_player` per player, so the
  arrays and host are always consistent.
* Severity LOW (only reachable via corrupted persistence). Classification BUG
  (missing invariant on the rehydrate seam). Proposed fix: have the repository
  assert host ∈ players (or document the trust boundary). Same applies to
  `Channel::rehydrate`.

---

## CHANNEL

### C-1 — a user can be in two channels at once (old channel not left on join) — HIGH — BUG
* original: `conn_set_channel` (connection.cpp:1955-1956) calls
  `conn_part_channel(c)` **before** adding the connection to the new channel —
  membership is mutually exclusive; you are always in exactly one channel.
* v3: `JoinChannel::execute` (join_channel.cpp) only `admit`s the account to the
  target channel and saves it. It never looks up or removes the account from any
  previously-joined channel. `LeaveChannel` is a separate explicit call.
  The connection FSM tracks a single `channel_id_` (connection_fsm_inchannel.cpp:
  84) but does NOT call LeaveChannel before the new join — `on_join_channel` goes
  straight to `join_channel_->execute(...)` (line 69) without leaving the old
  channel in the repository.
* Example v3 allows that original forbids: account joins "Channel A" (now a member
  of A in the repo), then joins "Channel B". In v3 the account is now a member of
  *both* A and B simultaneously (A still lists it, B lists it). The original
  guarantees only B. This corrupts member counts, `member_ids()` notifications,
  auto-delete (A never empties), and tempOP accounting.
* Severity HIGH (illegal state: simultaneous multi-channel membership the model
  is built to forbid). Classification BUG. Proposed fix: in `JoinChannel` (or the
  FSM before it), look up the caller's current channel and `leave` it first
  (emitting ChannelLeft + triggering auto-delete), exactly like
  `conn_part_channel`. The FSM already knows `channel_id_`; call LeaveChannel
  before JoinChannel when `channel_id_ != 0 && != target`.

### C-2 — first user of a non-permanent channel does NOT become operator (tempOP) — HIGH — NOT-IMPLEMENTED
* original: `channel_add_connection` (channel.cpp:462-471): when the channel is
  not permanent, not thevoid, not clan, `currmembers == 1`, and the joiner is not
  already op/admin, the joiner is granted **tempOP** (`conn_set_tmpOP_channel`)
  and userflags updated. This is the core "first person to make a channel runs
  it" rule.
* v3: `Channel::admit` (channel.hpp:110-137) has no operator concept at all. The
  aggregate stores no per-member op/voice flags. `OpFromChannel`
  (op_from_channel.cpp:43-50) explicitly notes "The Channel aggregate does not yet
  expose a dedicated grant_op / revoke_op command; the op flag is tracked
  externally" and is a structural no-op. There is no tempOP-on-create path.
* Example v3 allows that original forbids: user creates "MyChan" by joining it
  (channel auto-created). In the original they are now tempOP and can /kick,
  /ban, set topic. In v3 they have no operator status; a freshly created channel
  has *no* operators, so kick/ban/op/topic (all gated on the external "operator"
  command group) are impossible for the creator — the channel is un-moderatable.
* Severity HIGH (a missing invariant that leaves channels without their mandated
  first-operator; also makes the moderation use-cases inoperable for normal
  channels). Classification NOT-IMPLEMENTED. Proposed fix: model per-member flags
  (op/voice/tempOP) in `Channel`; in `admit`, when the policy is non-permanent /
  non-system / non-clan and this is the first member, mark them tempOP and emit
  an event. Re-point `OpFromChannel`/`set_topic`/`kick`/`ban` authorization at
  the aggregate's op set instead of the external command group.

### C-3 — max-members enforcement diverges: admin/op bypass missing & `max==0` semantics inverted — MEDIUM — BUG
* original: enforced in `conn_set_channel` (connection.cpp:1966-1980):
  - `channel_get_max(channel) == 0`  ⇒ **Admins/Operators only** channel
    (everyone else rejected "for Admins/Operators only").
  - `max != -1 && curr >= max` ⇒ rejected "currently full",
    **but admins and operators bypass both checks**.
  - `max == -1` ⇒ unlimited.
* v3: `Channel::is_full()` (channel.hpp:88-90): `max_members != 0 && size >= max`,
  i.e. **`max_members == 0` means UNLIMITED** (channel.hpp:56 comment "0 ⇒
  unlimited"). There is no admin/operator bypass — `admit` rejects everyone when
  full (channel.hpp:126-130).
* Divergences:
  1. `max==0` means *unlimited* in v3 but *admin-only* in the original — opposite
     meanings. A channel persisted/configured with `max=0` behaves inversely.
  2. No admin/operator override: in the original an admin can always enter a full
     channel; in v3 `admit` returns `Full` for everyone including admins/ops.
* Example v3 allows/forbids: (a) `max=0` channel — original admits only ops; v3
  admits *everyone* unbounded. (b) full channel — original lets an admin in; v3
  rejects the admin.
* Severity MEDIUM (admission-policy invariant divergence; the `max==0` inversion
  can over-admit to an intended ops-only room). Classification BUG. Proposed fix:
  reconcile the sentinel: either adopt the original (`0`=ops-only, `-1`=unlimited)
  or keep `0`=unlimited but ensure the config loader translates correctly; and
  thread an "is admin/op" predicate into `admit` so privileged users bypass Full.

### C-MATCH-1 — temporary channel auto-deletes when last member leaves; permanent persists — MATCHES
* original: `channel_del_connection` (channel.cpp:558): `if (!memberlist &&
  !(flags & permanent)) channel_destroy(...)`.
* v3: `LeaveChannel` (leave_channel.cpp:40-48): `should_delete = member_count==0
  && !flags.has(Permanent)` → `channel_repo_.remove`.
* Verdict: MATCH (permanent channels are kept even when empty; temporary ones are
  removed). Good.

### C-MATCH-2 — banned user rejected on join — MATCHES
* original: `channel_check_banning` rejects (channel.cpp:446-450) and
  `conn_set_channel` rejects banned (connection.cpp:1960-1964).
* v3: `admit` checks `is_banned` → `JoinOutcome::Banned` (channel.hpp:116-120).
* Verdict: MATCH.

### C-MATCH-3 — only members can talk; non-members' posts rejected — MATCHES
* original: messages route only to channel members; non-members aren't in the
  member loop.
* v3: `Channel::post` returns false unless `contains(from)` (channel.hpp:147).
* Verdict: MATCH.

### C-4 — moderated flag does NOT gate talking in v3 — MEDIUM — NOT-IMPLEMENTED
* original: `channel_message_send` (channel.cpp:694-705): in a `moderated`
  channel, `talk`/`emote` are dropped ("This channel is moderated") unless the
  sender is op/admin/voice/tempVOICE. (Also: muted accounts and the `thevoid`/
  silent flag block talk, lines 680-691.)
* v3: `Channel::post` (channel.hpp:146-150) checks membership only. It ignores
  `ChannelFlag::Moderated`, `Silent`, and has no voice/mute concept. Any member
  can talk in a moderated channel.
* Example v3 allows that original forbids: a moderated channel; an ordinary
  (non-op, non-voice) member sends a TALK → v3 accepts and broadcasts; original
  drops it with "This channel is moderated."
* Severity MEDIUM (a documented flag with no effect ⇒ moderated channels behave
  as unmoderated). Classification NOT-IMPLEMENTED. Proposed fix: in `post`, if
  `flags.has(Moderated)` and the sender is not op/voice, reject; also honour
  `Silent`/mute once per-member voice/mute flags exist (ties into C-2).

### C-5 — `Restricted` flag is defined but unused — LOW — NOT-IMPLEMENTED / UNSURE
* original: the channel flag set drives who may join/talk (moderated, thevoid,
  permanent, clan, etc.). v3 defines `ChannelFlag::Restricted` (channel.hpp:32)
  but no code reads it (`admit`/`post` never test it).
* Severity LOW. Classification NOT-IMPLEMENTED/UNSURE — confirm what `Restricted`
  was intended to mean (the original has no exact `restricted` flag; closest are
  `moderated` for talk and `max==0` for ops-only join). Proposed fix: either wire
  it to a concrete admission/talk rule or remove the dead flag.

### C-6 — `kick`/`ban` can remove the last operator; no "last operator" protection — MEDIUM — BUG/NOT-IMPLEMENTED
* prompt asks: "can't kick the last operator." Because v3 has **no operator
  concept in the aggregate** (see C-2), there is nothing stopping the removal of
  the last moderator. `Channel::kick` (channel.hpp:154-160) removes any target
  and banlists them with only a "moderator is a member" check. The application
  layer (kick_from_channel.cpp:44-47, ban_from_channel.cpp:39-42) refuses to
  kick/ban anyone holding the global "operator"/"admin" command group, which is a
  blunt global immunity, not a per-channel "don't remove the last op" rule.
* original: operator/admin status is per-channel (tmpOP + account auth);
  combined with C-2 the channel always retains a moderator (the auto-tempOP rule
  re-grants when needed). The original's kick refuses ops/admins too.
* Divergence: v3 cannot express "this channel must keep ≥1 operator" because it
  has no per-channel op set. With the *global* immunity, a channel whose only op
  is a plain tempOP (no global group) could have that op kicked, leaving the
  channel leaderless — the original would have re-assigned tempOP.
* Severity MEDIUM. Classification BUG/NOT-IMPLEMENTED (depends on C-2). Proposed
  fix: track per-channel operators in the aggregate and reject a kick/ban that
  would remove the last operator (or auto-promote the next member, mirroring the
  tempOP rule).

### C-MATCH-4 — kick adds non-member targets to banlist (legacy quirk) — MATCHES
* v3 `kick` (channel.hpp:154-160) erases the target if present and banlists it
  regardless. The comment cites "legacy behaviour"; the original likewise lets
  ban target an absent user. MATCH (intentional parity).

### C-7 — channel-name client-tag admission: parity OK but case-/realm-folding absent — LOW — UNSURE
* original: channels are looked up with country + realm folding and per-clienttag
  rollover ("Starcraft USA-1"); admission can be tag-restricted.
* v3: `admit` enforces `policy_.client` tag match (channel.hpp:121-125) — the
  single-tag restriction MATCHES in spirit. The auto-rollover / country / realm
  naming logic is not modelled (JoinChannel auto-creates an unlimited public
  channel, join_channel.cpp:31-39). Severity LOW, NOT-IMPLEMENTED for rollover;
  the core tag gate MATCHES.

---

## Summary table

| ID | Area | Severity | Class | One-liner |
|----|------|----------|-------|-----------|
| G-MATCH-1 | join when open+not-full | — | MATCH | enforced both sides |
| G-MATCH-2 | duplicate join | — | MATCH | v3 stricter (AlreadyIn) |
| G-MATCH-3 | host counts as player | — | MATCH | |
| G-MATCH-4 | no-backwards / no double finalize/start | — | MATCH | |
| G-MATCH-5 | host migration on leave | — | MATCH | earliest survivor, recently fixed |
| G-1 | max_players cap 1..16 | LOW | UNSURE | original unbounded |
| G-2 | no `Full`/locked lobby state | MEDIUM | INTENTIONAL | host can't lock a non-full lobby; latent |
| G-3 | no `loaded` resume state | LOW | NOT-IMPL | |
| G-4 | 1-player game not torn down (orig `ref<2`) | MEDIUM | BUG | zombie game, no result |
| G-5 | any player finalizes instantly | LOW | UNSURE | results-integrity |
| G-6 | rehydrate skips invariants | LOW | BUG | corrupt persistence ⇒ illegal aggregate |
| C-1 | user in two channels at once | HIGH | BUG | join doesn't leave old channel |
| C-2 | first user not made operator | HIGH | NOT-IMPL | channels un-moderatable |
| C-3 | max-members: `0` inverted, no admin bypass | MEDIUM | BUG | over/under admission |
| C-MATCH-1 | temp auto-delete / perm persist | — | MATCH | |
| C-MATCH-2 | banned rejected | — | MATCH | |
| C-MATCH-3 | only members talk | — | MATCH | |
| C-4 | moderated flag doesn't gate talk | MEDIUM | NOT-IMPL | moderated == unmoderated |
| C-5 | `Restricted` flag unused | LOW | UNSURE | dead flag |
| C-6 | last-operator removable | MEDIUM | BUG/NOT-IMPL | leaderless channel |
| C-MATCH-4 | kick banlists absent target | — | MATCH | legacy parity |
| C-7 | clienttag gate ok; rollover absent | LOW | UNSURE | |

Highest-priority: **C-1** (two-channels-at-once) and **C-2** (no first-user
operator) are HIGH and both stem from the channel join path / missing operator
model. **G-4** (zombie 1-player game) and **C-3** (max-members semantics) are the
notable MEDIUM lifecycle/admission divergences.

## Wave 52: advertised game must be torn down on host disconnect
Verified differentially (tests/diff/diff_game_disconnect.py): when a BNCS host
advertises via SID_STARTADVEX3 and then disconnects without SID_CLOSEGAME, the
oracle removes the game from GETADVLISTEX; v3 ghosted it. Fixed by wiring the
LeaveGame use-case (it was declared in BnetUseCaseContext but never constructed in
main.cpp -> null, which also silently broke explicit SID_CLOSEGAME) and by having
BnetFsm::on_disconnect run leave_game for current_game_id_, mirroring on(CloseGame).
Invariant: a connection's hosted game is removed when that connection is destroyed,
exactly as the original's conn_destroy -> game cleanup.
