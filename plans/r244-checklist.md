# R244 checklist -- bnetd IP-ban subsystem observation bridges

## Status
GREEN. 10 cases / 42 assertions.

## Scope
Continue bnetd strangler. Selected `src/bnetd/ipban.cpp` -- the
IP-ban subsystem. Seven entry points wired through new
`integration_legacy_bnetd/ipban_bridge`:

* `ipbanlist_create()`         -- one-shot startup init.
* `ipbanlist_destroy()`        -- one-shot shutdown drain.
* `ipbanlist_load(filename)`   -- admin reload.
* `ipbanlist_save(filename)`   -- admin persist.
* `ipbanlist_check(addr)`      -- per-connect lookup.
* `ipbanlist_add(c, addr, t)`  -- admin add (owner reduced
                                  to socket descriptor).
* `ipbanlist_unload_expired()` -- periodic sweep.

Deferred: `ipbanlist_str_to_time_t`, `handle_ipban_command` --
the former is a pure parser (no behavioural side-effects worth
observing); the latter is an admin command-dispatch entry that
belongs with the broader command-dispatcher bridges.

## Design notes
* Single new module `v3_bnetd_ipban_bridge`. C symbol prefix
  `pvpgn_v3_bnetd_ipban_*_try` reuses the bnetd daemon
  convention.
* `t_connection*` owner is reduced to its raw socket descriptor
  via `conn_get_socket(c)` at the legacy call site; `-1` is
  used as the sentinel for "no admin context" (e.g. script-
  initiated ban load).
* `std::time_t endtime` widened to `unsigned long long` at the
  ABI boundary.
* Null `const char*` arguments are tolerated and rendered as
  the literal `<null>` via `safe_str`.
* Log levels follow the cadence heuristic:
  - `create` / `destroy` / `unload_expired` -> `Debug`
    (startup/shutdown/periodic).
  - `load` / `save` / `add`                 -> `Info`
    (admin operations -- low frequency, audit-worthy).
  - `check`                                  -> `Trace`
    (per-connect lookup, hot path).
* Bridge calls are placed *after* the existing NULL-argument
  guards in each function (so the legacy semantics of
  `-1`-on-NULL is preserved unchanged) and *before* the
  mutation/IO step. For `add`, the bridge runs before the
  parser so the original `cp` string is observed regardless
  of whether `ipban_str_to_ipban_entry` would have rejected it.

## Files added
* `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/ipban_bridge.hpp`
* `src/v3/integration/legacy_bnetd/src/ipban_bridge.cpp`
* `tests/unit/integration/legacy_bnetd/ipban_bridge_test.cpp`
* `plans/r244-checklist.md`

## Files modified
* `src/bnetd/ipban.cpp`
  - Added `PVPGN_V3_BNETD_INTEGRATION`-guarded forward decls
    after `setup_after.h`.
  - Added seven guarded bridge call sites (one per entry
    point), placed after NULL-guards and before mutation.
* `src/v3/CMakeLists.txt`
  - Appended `integration/legacy_bnetd/src/ipban_bridge.cpp`
    to `integration_legacy_bnetd` SOURCES.
* `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Registered `test_integration_legacy_bnetd_ipban_bridge`.
* `Dockerfile.v3`
  - Appended `test_integration_legacy_bnetd_ipban_bridge` to
    the CI build-target list (~line 63).
  - Appended a `--reporter compact` invocation to the v3-test
    RUN chain.

## Verify
```
cmake --build build --target \
    integration_legacy_bnetd \
    test_integration_legacy_bnetd_ipban_bridge \
    bnetd_legacy bnetd -j$(nproc)

./build/tests/unit/integration/legacy_bnetd/test_integration_legacy_bnetd_ipban_bridge --reporter compact
# All tests passed (42 assertions in 10 test cases)
```
`bnetd_legacy` and `bnetd` both relink cleanly.

## Lessons
* The first `cmake --build` invocation after editing a tests
  CMakeLists triggers config-regenerate, but the *initial*
  pass may still hit `No rule to make target` because the
  driving make file resolves test target dependencies before
  reading the freshly generated rules. A second
  `cmake --build` invocation (or building the test target
  alone) reliably picks it up.
* Use `conn_get_socket(c)` (not `(uintptr_t)c`) when collapsing
  legacy connection pointers to bridge scalars -- the
  descriptor is the stable, diagnostically useful key.
* For "admin op" bridges (load / save / add), Info level is
  the right cadence: low frequency, but every event is
  audit-worthy in production logs.
