# R240 -- d2cs per-game character add/del observation bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_game_bridge`)

## Scope

Eighth strangler-fig round in `src/d2cs/`. Closes out the legacy
game.cpp module by wrapping the two character-roster mutators:

| # | Symbol                       | Bridge                                                    |
|---|------------------------------|-----------------------------------------------------------|
| 1 | `game_add_character(g,n,c,l)`| `pvpgn_v3_d2cs_game_add_character_try(gid,n,c,l)`         |
| 2 | `game_del_character(g,n)`    | `pvpgn_v3_d2cs_game_del_character_try(gid,n)`             |

After R238 + R239 + R240 the d2cs game module is fully observed:
catalogue lifecycle, per-game lifecycle, d2gs binding triad, and
per-character roster mutations.

## Design notes

- Both bridges extend the existing `game_bridge.{hpp,cpp}` pair (no
  new files); shared module `v3_d2cs_game_bridge`; distinct
  messages.
- `add_character` is observation-before-classification: the bridge
  fires BEFORE the legacy update-or-insert branch. v3 can correlate
  by `(game_id, charname)` and infer whether it was an update or a
  fresh add by tracking prior bridge events.
- `del_character` is observation-before-lookup: the bridge fires
  BEFORE the legacy find-or-fail check. This matches the asymmetry
  established in R238 (create observes outcome, destroy observes
  intent) -- a `del` that subsequently fails with "character not
  found" still surfaces in the v3 trace, attributable via the
  eventlog error line that follows.
- Both bridges log at `Info`, matching the
  `eventlog_level_info` lines they sit next to in legacy.
- `chclass` and `level` are widened from `unsigned char` to
  `unsigned int` in the bridge ABI to keep the C-symbol signature
  uniform with every other uint scalar across the integration
  layer.

## Files added

- *(none)*

## Files modified

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/game_bridge.hpp`
  -- two new `extern "C"` declarations.
- `src/v3/integration/legacy_d2cs/src/game_bridge.cpp` -- two new
  bridge bodies sharing the existing `render_uint` + `safe_str`
  helpers.
- `src/d2cs/game.cpp` -- two new forward decls appended to the
  existing `PVPGN_V3_D2CS_INTEGRATION` block; guarded calls at the
  top of `game_add_character()` and `game_del_character()`, each
  right after the `ASSERT(charname,-1)` and before any find/insert
  logic.
- `tests/unit/integration/legacy_d2cs/game_bridge_test.cpp` -- four
  new `TEST_CASE`s: add happy path, add null name, del happy path,
  del null name.

## Verify

```sh
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_game_bridge \
    d2cs_legacy d2cs -j$(nproc)

./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_game_bridge \
    --reporter compact
```

Result: 15 TEST_CASEs, 69 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean. No Dockerfile.v3 changes required -- the same
`test_integration_legacy_d2cs_game_bridge` target already runs.

## Lessons (memorialised)

- When a legacy update-or-insert function takes the form
  `find(); if found update else insert`, the bridge fires before
  the find so a single observation captures BOTH paths. Pushing
  per-branch bridges would force v3 to maintain branch-specific
  state with no semantic benefit.
- Widening sub-int scalars (`unsigned char`, `unsigned short`) to
  `unsigned int` at the ABI boundary keeps the bridge surface
  uniform. The legacy side does a `static_cast<unsigned int>(x)`
  which is well-defined for all unsigned widening; the v3 side
  renders + interprets as if it were always uint32 with no loss.
- One module's bridges accumulate in a single `*_bridge.{hpp,cpp}`
  pair. By R240 the d2cs game_bridge pair ships 9 symbols across
  4 distinct concerns (catalogue, lifecycle, d2gs link, roster).
  Extension stays cheap because every new symbol reuses the same
  helpers (`render_uint`, `safe_str`) and the same module string.
