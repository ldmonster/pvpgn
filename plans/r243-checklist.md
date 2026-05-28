# R243 checklist -- bnetd timer subsystem observation bridges

## Status
GREEN. 5 cases / 30 assertions.

## Scope
Pivot back to bnetd. Pick a fresh, fully un-bridged module.
Selected: `src/bnetd/timer.cpp` -- the per-connection timer
subsystem. Five entry points wired through new
`integration_legacy_bnetd/timer_bridge`:

* `timerlist_create()`             -- one-shot startup init.
* `timerlist_destroy()`            -- one-shot shutdown drain.
* `timerlist_add_timer(owner, when, cb, data)` -- per-arming insert.
* `timerlist_del_all_timers(owner)`            -- per-connection cancel.
* `timerlist_check_timers(when)`               -- per-tick fire scan.

## Design notes
* Single new module `v3_bnetd_timer_bridge`. C symbol prefix
  `pvpgn_v3_bnetd_timerlist_*_try` reuses the established
  bnetd daemon convention.
* `t_connection*` owner is reduced to its raw socket descriptor
  via `conn_get_socket(owner)` at the legacy call site so the
  v3 ABI stays POD-only. `t_timer_cb` / `t_timer_data` are
  *not* exposed -- they are internal legacy-side concerns.
* `std::time_t` is widened to `unsigned long long` at the ABI
  boundary (same widening rule as the d2cs setters).
* Log levels follow the cadence heuristic:
  - `create` / `destroy`           -> `Debug` (one-shot, but
    not per-process; the daemon may re-init on reload).
  - `add_timer` / `check_timers`   -> `Trace` (per-tick / per-arm).
  - `del_all_timers`               -> `Debug` (per-conn teardown).
* Bridge calls are placed *after* the existing NULL-owner
  guard in the two functions that accept an owner, so the
  legacy contract is preserved. `create` runs before the
  legacy `elist_init` so the observation reflects intent;
  `destroy` likewise runs before the drain loop.

## Files added
* `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/timer_bridge.hpp`
* `src/v3/integration/legacy_bnetd/src/timer_bridge.cpp`
* `tests/unit/integration/legacy_bnetd/timer_bridge_test.cpp`
* `plans/r243-checklist.md`

## Files modified
* `src/bnetd/timer.cpp`
  - Added `PVPGN_V3_BNETD_INTEGRATION`-guarded forward decls
    after `setup_after.h`.
  - Added five guarded bridge call sites (one per entry point).
* `src/v3/CMakeLists.txt`
  - Appended `integration/legacy_bnetd/src/timer_bridge.cpp`
    to `integration_legacy_bnetd` SOURCES.
* `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Registered `test_integration_legacy_bnetd_timer_bridge`.
* `Dockerfile.v3`
  - Appended `test_integration_legacy_bnetd_timer_bridge` to
    the CI build-target list (~line 63).
  - Appended a `--reporter compact` invocation to the v3-test
    RUN chain.

## Verify
```
cmake --build build --target \
    integration_legacy_bnetd \
    test_integration_legacy_bnetd_timer_bridge \
    bnetd_legacy bnetd -j$(nproc)

./build/tests/unit/integration/legacy_bnetd/test_integration_legacy_bnetd_timer_bridge --reporter compact
# All tests passed (30 assertions in 5 test cases)
```
`bnetd_legacy` and `bnetd` both relink cleanly with the new
guarded forward decls and call sites.

## Lessons
* `extern int` declarations in `src/bnetd/*.cpp` are indented
  inside the `pvpgn::bnetd` namespace block, so a `^extern`
  grep regex returns no matches. Use `^\s*extern` or read the
  file directly when surveying legacy entry-point shape.
* For owner-of-connection scalars, prefer `conn_get_socket()`
  over `(uintptr_t)owner` -- the descriptor is meaningful in
  logs and stable across the v3/legacy seam, whereas pointer
  bits leak ASLR detail with no diagnostic value.
* When a legacy `.cpp` has *no* existing
  `PVPGN_V3_BNETD_INTEGRATION` block, the new forward decls
  go in a freshly-opened guarded block placed *after*
  `setup_after.h` (not inside the `pvpgn::bnetd` namespace --
  the C symbols must live at the file's outermost scope).
