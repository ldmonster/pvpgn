# R245 checklist -- bnetd small-module lifecycle batch

## Status
GREEN. 11 cases / 49 assertions.

## Scope
User asked for "all bnet modules" -- begin by batching the
small lifecycle / dispatch entry points into a single bridge
unit. Five modules covered in this round:

| Module     | Bridged entry points                         |
|------------|----------------------------------------------|
| helpfile   | `helpfile_init`, `helpfile_unload`            |
| autoupdate | `autoupdate_load`, `autoupdate_unload`        |
| output     | `output_init`, `output_write_to_file`         |
| support    | `support_check_files`                         |
| mail       | `handle_mail_command`, `check_mail`           |

Nine entry points total, all C-compatible POD ABIs.

## Design notes
* Single combined bridge file `bnetd_lifecycle_bridges.{hpp,cpp}`
  with per-function module strings (`v3_bnetd_<mod>_bridge`) so
  log filters still see each subsystem distinctly.
* `t_connection*` arguments collapsed to `conn_get_socket(c)`
  (sentinel `-1` for no-conn). `t_connection const *` paths
  pass the same accessor.
* Levels: bulk init/unload of admin-loaded config = `Info`
  (audit-worthy); periodic / hot-path observers (`output_init`
  on start, `output_write_to_file` on save, `mail check`) =
  `Debug` / `Trace`.

## Files added
* `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/bnetd_lifecycle_bridges.hpp`
* `src/v3/integration/legacy_bnetd/src/bnetd_lifecycle_bridges.cpp`
* `tests/unit/integration/legacy_bnetd/bnetd_lifecycle_bridges_test.cpp`
* `plans/r245-checklist.md`

## Files modified
* `src/bnetd/helpfile.cpp`     -- 2 guarded call sites.
* `src/bnetd/autoupdate.cpp`   -- 2 guarded call sites.
* `src/bnetd/output.cpp`       -- 2 guarded call sites.
* `src/bnetd/support.cpp`      -- 1 guarded call site.
* `src/bnetd/mail.cpp`         -- 2 guarded call sites.
* `src/v3/CMakeLists.txt`      -- new SOURCES entry.
* `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  -- new test registration.
* `Dockerfile.v3` -- target list + RUN chain entry.

## Verify
```
cmake --build build --target \
    integration_legacy_bnetd \
    test_integration_legacy_bnetd_lifecycle_bridges \
    bnetd_legacy bnetd -j$(nproc)
./build/tests/unit/integration/legacy_bnetd/test_integration_legacy_bnetd_lifecycle_bridges --reporter compact
# All tests passed (49 assertions in 11 test cases)
```

## Lessons
* Bundling several small lifecycle observers into one .hpp/.cpp
  bridge unit (with per-subsystem module strings) cuts CMake/
  test boilerplate ~5x compared to one bridge per module. Logs
  remain filterable; tests stay readable. Good pattern for
  small modules. Keep one-bridge-per-module discipline for any
  module with >=4 entry points or non-trivial state.
* `output_init` returns void; the bridge still returns `int`
  (always 0) -- the legacy call site simply does
  `(void)pvpgn_v3_..._try()` so the void/int distinction
  doesn't leak into the macro guard.
