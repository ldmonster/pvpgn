# R239 -- d2cs per-game setter triad observation bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_game_bridge`)

## Scope

Seventh strangler-fig round in `src/d2cs/`. Wraps the three per-game
state mutators in `src/d2cs/game.cpp` that link the d2cs-side game
entity to its upstream d2gs and toggle the "created" handshake flag:

| # | Symbol                                | Bridge                                                |
|---|---------------------------------------|-------------------------------------------------------|
| 1 | `game_set_d2gs_gameid(g, gid)`        | `pvpgn_v3_d2cs_game_set_d2gs_gameid_try(gid, dgid)`   |
| 2 | `game_set_d2gs(g, gs)`                | `pvpgn_v3_d2cs_game_set_d2gs_try(gid, d2gs_id)`       |
| 3 | `game_set_created(g, c)`              | `pvpgn_v3_d2cs_game_set_created_try(gid, c)`          |

Combined with R238 the entire d2cs game lifecycle is now observed:
catalogue create/destroy, per-game create/destroy, and the three
linkage / state mutations that bridge d2cs <-> d2gs.

## Design notes

- All three symbols extend the existing `game_bridge.{hpp,cpp}` pair
  (no new files); shared module `v3_d2cs_game_bridge`, distinct
  messages.
- `game_set_d2gs_gameid` and `game_set_d2gs` log at `Debug` (cold,
  per-handshake events). `game_set_created` logs at `Info` because
  the 0->1 transition is the canonical "d2gs ack'd the game"
  milestone -- it's the single most useful trace event for
  diagnosing failed game spawns.
- `game_set_d2gs(g, gs)` receives a legacy `t_d2gs*` -- the bridge
  invariant requires POD scalars only, so the call site extracts
  `d2gs_get_id(gs)` (or 0 when `gs` is null to signal detach) and
  passes that uint32 instead.
- Each bridge call sits AFTER `ASSERT(game, -1)` so legacy guards
  always run first; v3 only sees calls with a valid `game` pointer.

## Files added

- *(none)*

## Files modified

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/game_bridge.hpp`
  -- three new `extern "C"` declarations.
- `src/v3/integration/legacy_d2cs/src/game_bridge.cpp` -- three new
  bridge bodies sharing the existing `render_uint` helper.
- `src/d2cs/game.cpp` -- three new forward decls appended to the
  existing `PVPGN_V3_D2CS_INTEGRATION` block; guarded calls at the
  top of `game_set_d2gs_gameid()`, `game_set_d2gs()`, and
  `game_set_created()`, each right after the `ASSERT(game, -1)` and
  before the actual field write.
- `tests/unit/integration/legacy_d2cs/game_bridge_test.cpp` -- four
  new `TEST_CASE`s: `game_set_d2gs_gameid` happy path,
  `game_set_d2gs` non-null + null (detach) cases, `game_set_created`
  happy path.

## Verify

```sh
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_game_bridge \
    d2cs_legacy d2cs -j$(nproc)

./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_game_bridge \
    --reporter compact
```

Result: 11 TEST_CASEs, 50 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean. No Dockerfile.v3 changes required -- the same
`test_integration_legacy_d2cs_game_bridge` target already runs in
the v3-test RUN chain since R238.

## Lessons (memorialised)

- Setter functions with a single field write are EXCELLENT bridge
  targets: their semantic is "this scalar changed", so the bridge
  ABI maps 1:1 onto the legacy signature with no decoding required.
- When a setter takes a pointer to another legacy struct (`t_d2gs*`),
  extract the relevant ID on the legacy side (here: `d2gs_get_id(gs)`)
  rather than dragging the struct across the bridge. Pass 0 (or
  another sentinel) for null. This keeps the bridge POD-only.
- Choose log level by signal-to-noise: setters that fire many times
  during normal traffic (gameid, d2gs binding) stay at `Debug`;
  setters that mark a once-per-game state transition (`created`
  going 0->1) get `Info` so they survive default log filtering.
