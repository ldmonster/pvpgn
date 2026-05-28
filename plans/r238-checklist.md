# R238 -- d2cs game catalogue + per-game lifecycle bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_game_bridge`)

## Scope

Sixth strangler-fig round in `src/d2cs/`. Wraps the game catalogue
lifecycle (process-wide singleton) AND the per-game create / destroy
operations in `src/d2cs/game.cpp`:

| # | Symbol                           | Bridge                                                    |
|---|----------------------------------|-----------------------------------------------------------|
| 1 | `d2cs_gamelist_create()`         | `pvpgn_v3_d2cs_gamelist_create_try()`                     |
| 2 | `d2cs_gamelist_destroy()`        | `pvpgn_v3_d2cs_gamelist_destroy_try()`                    |
| 3 | `d2cs_game_create(...)`          | `pvpgn_v3_d2cs_game_create_try(id, name, flag)`           |
| 4 | `game_destroy(game, elem)`       | `pvpgn_v3_d2cs_game_destroy_try(id, name)`                |

After R238 the d2cs game-state lifecycle is fully observed: catalogue
construction, individual game creation (with assigned id + gameflag),
and per-game teardown.

## Design notes

- All four symbols live in a single bridge pair
  (`game_bridge.{hpp,cpp}`), module string `v3_d2cs_game_bridge`,
  messages disambiguate (`gamelist create observed`,
  `gamelist destroy observed`, `game create observed`,
  `game destroy observed`).
- `game_create` fires AFTER `list_prepend_data` + `total_game++` so
  v3 only sees committed creations -- failed creates (duplicate
  name) return `NULL` from `d2cs_gamelist_find_game` and never reach
  the bridge. The `id` field is always non-zero (legacy guards
  against rollover wrapping to 0).
- `game_destroy` fires at the TOP of the function, BEFORE the
  `list_remove_data` call, so v3 sees every destroy attempt
  (including ones that subsequently fail). This is intentional
  asymmetry: create reflects outcome, destroy reflects intent.
- Both create and destroy log at `Info`, matching the
  `eventlog_level_info` lines they sit next to. Catalogue lifecycle
  is `Debug` -- fires once at startup/shutdown only.
- The bridge accepts `gameflag` as decimal uint32. The legacy
  eventlog renders it as `0x{:08X}`; v3 readers wanting hex can
  reformat. Decimal keeps the bridge ABI uniform with all other
  uint fields in the codebase.

## Files added

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/game_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/game_bridge.cpp`
- `tests/unit/integration/legacy_d2cs/game_bridge_test.cpp`

## Files modified

- `src/v3/CMakeLists.txt` -- one new source wired into
  `integration_legacy_d2cs`.
- `src/d2cs/game.cpp` -- new forward-decl block (after
  setup_after.h) declaring all four bridge symbols under
  `PVPGN_V3_D2CS_INTEGRATION`; guarded calls in
  `d2cs_gamelist_create()`, `d2cs_gamelist_destroy()`,
  `d2cs_game_create()` (right after `total_game++`), and
  `game_destroy()` (right after `ASSERT(game,-1)`).
- `tests/unit/integration/legacy_d2cs/CMakeLists.txt` -- one new
  `pvpgn_v3_add_test(...)` entry.
- `Dockerfile.v3` -- new target added to both the explicit
  `cmake --build --target ...` list and the v3-test RUN chain.

## Verify

```sh
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_game_bridge \
    d2cs_legacy d2cs -j$(nproc)

./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_game_bridge \
    --reporter compact
```

Result: 7 TEST_CASEs, 31 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean.

## Lessons (memorialised)

- For per-entity lifecycle bridges where the entity has an ID
  assigned during create, place the create bridge AFTER the ID
  assignment so v3 receives the canonical id along with the name.
  Placing it before would force v3 readers to correlate via name,
  which is racy under future concurrent-create scenarios.
- Create-after-commit vs destroy-before-mutation is a deliberate
  asymmetry: v3 sees committed creates (no phantom entities) and
  attempted destroys (so a destroy that subsequently fails still
  shows up in the trace pipeline, attributable via the eventlog
  error line that follows). This matches the "fail-loud, observe-
  early" doctrine.
- When the legacy struct field offers it (`game->id`, `game->name`),
  pass scalars + null-safe const-strings rather than the struct
  pointer. The bridge ABI stays POD-only, the v3 side never needs
  to know `t_game`'s layout, and the strangler-fig boundary remains
  clean.
