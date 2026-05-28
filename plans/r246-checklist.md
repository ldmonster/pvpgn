# R246 — bnetd small-module lifecycle bridges (second batch)

Status: ✅ GREEN — 16 cases / 59 assertions.

## Scope

Strangler-fig observation bridges over six more bnetd legacy
modules; all entry points take POD scalars or null-safe C strings
only, so the ABI surface stays trivial.

| Module      | Symbol(s)                                                                                              | Level |
| ----------- | ------------------------------------------------------------------------------------------------------ | ----- |
| i18n        | `_i18n_load_try`, `_i18n_reload_try`                                                                   | Info  |
| icons       | `_icons_load_try(filename)`, `_icons_unload_try`                                                       | Info  |
| attrlayer   | `_attrlayer_init_try`, `_attrlayer_cleanup_try`, `_attrlayer_save_try(flags)`, `_attrlayer_flush_try(flags)` | Debug/Info |
| tracker     | `_tracker_set_servers_try(servers)`, `_tracker_send_report_try`                                        | Info/Trace |
| team        | `_team_load_try`, `_team_unload_try`                                                                   | Debug |
| udptest     | `_udptest_send_try(sd)`                                                                                | Trace |

Per-function module strings `v3_bnetd_<mod>_bridge` so log filters
still see individual subsystems even though all entry points are
co-located in one translation unit.

## Files added

- src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/bnetd_lifecycle_bridges_r246.hpp
- src/v3/integration/legacy_bnetd/src/bnetd_lifecycle_bridges_r246.cpp
- tests/unit/integration/legacy_bnetd/bnetd_lifecycle_bridges_r246_test.cpp
- plans/r246-checklist.md

## Files modified (legacy wiring)

- src/bnetd/i18n.cpp — guarded calls in `i18n_load`, `i18n_reload`.
- src/bnetd/icons.cpp — guarded calls in `customicons_load`, `customicons_unload`.
- src/bnetd/attrlayer.cpp — guarded calls in `attrlayer_init`,
  `attrlayer_cleanup`, `attrlayer_save`, `attrlayer_flush`.
- src/bnetd/tracker.cpp — guarded calls in `tracker_set_servers`,
  `tracker_send_report`.
- src/bnetd/team.cpp — guarded calls in `teamlist_load`,
  `teamlist_unload`.
- src/bnetd/udptest_send.cpp — guarded call in `udptest_send`
  passing `c ? conn_get_socket(c) : -1`.

All wires are gated by `PVPGN_V3_BNETD_INTEGRATION`; legacy paths
fall through unchanged.

## Files modified (build / CI)

- src/v3/CMakeLists.txt — added
  `integration/legacy_bnetd/src/bnetd_lifecycle_bridges_r246.cpp`
  to `integration_legacy_bnetd` SOURCES.
- tests/unit/integration/legacy_bnetd/CMakeLists.txt — registered
  `test_integration_legacy_bnetd_lifecycle_bridges_r246`.
- Dockerfile.v3 — added target to both the v3 build target list
  (line ~63) and the v3-test RUN chain (line ~265).

## Verification

```
$ cmake --build build -j8 --target \
    integration_legacy_bnetd \
    test_integration_legacy_bnetd_lifecycle_bridges_r246 \
    bnetd_legacy bnetd
$ ./build/tests/unit/integration/legacy_bnetd/\
test_integration_legacy_bnetd_lifecycle_bridges_r246 --reporter compact
All tests passed (59 assertions in 16 test cases)
```

`bnetd` and `bnetd_legacy` link cleanly with the new guarded wires.

## Lessons applied

- Forward decls placed AFTER `common/setup_after.h` at outermost
  scope (not inside `pvpgn::bnetd` namespace).
- POD-only ABI; pointer args resolved on legacy side via
  `conn_get_socket(c)` with `-1` sentinel for null.
- Integers rendered via `std::to_chars` into local
  `std::array<char, 24>`.
- Bridge registered in PARENT `integration_legacy_bnetd` lib
  SOURCES.
- "No rule to make target" sidestepped by invoking `cmake --build`
  for the new test target after the initial pass.
- `udptest_send.cpp` already uses a separate
  `send_udptest_bridge.hpp`; R246 adds an additional lifecycle
  bridge without touching the existing observation hook.
