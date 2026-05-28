# R231 -- Observation lifecycle bridges for d2dbs (charlock + d2ladder)

Status: **GREEN** (local Linux build: `cmake --build build --target d2dbs d2dbs_legacy test_integration_legacy_d2dbs_charlock_bridge test_integration_legacy_d2dbs_d2ladder_bridge`)

## Scope

First strangler-fig round in the `src/d2dbs/` tree, mirroring R230's
playbook for `src/bnetd/` (`runprog`, `news`, `userlog`,
`versioncheck`). Adds two observation-only bridges plus the shared
`legacy_d2dbs::bridge_logger` seam that future d2dbs bridges will
share.

Each new bridge:

1. Adds a header in
   `src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/`.
2. Adds a `.cpp` returning 0 (legacy fall-through) and emitting a
   structured kv log via `pld::bridge_log_kv`.
3. Is wired into the `integration_legacy_d2dbs` parent lib in
   `src/v3/CMakeLists.txt` (NOT `integration_legacy_d2dbs_linked` --
   observation bridges have no legacy-header dependency).
4. Is invoked from the legacy site under
   `#ifdef PVPGN_V3_D2DBS_INTEGRATION` with a forward declaration
   placed AFTER `common/setup_after.h` (mirrors the
   `src/bnetd/news.cpp` template introduced in R230).
5. Ships with a Catch2 unit test under
   `tests/unit/integration/legacy_d2dbs/`.
6. Is registered in `Dockerfile.v3` in two places: the explicit
   `cmake --build --target ...` list AND the `v3-test` RUN chain.

## Targets

| # | Module        | Op(s)                                              | Status |
|---|---------------|----------------------------------------------------|--------|
| 1 | charlock.cpp  | `cl_init(tbllen, maxgs)`, `cl_destroy()`           | [x]    |
| 2 | d2ladder.cpp  | `d2dbs_d2ladder_init()`, `d2dbs_d2ladder_destroy()`| [x]    |

Out of scope for R231 (deferred):

- `dbsdupecheck.cpp` -- single hot-path `dbsdupecheck()` op with no
  lifecycle phase; its bridge belongs with the per-packet
  observation set, not the load/unload set.
- `dbserver.cpp` -- `dbs_server_main()` is the d2dbs main loop;
  instrumenting it requires more than a single-line `try` hook and
  is its own round.
- `handle_signal.cpp` -- signal handlers; deferred until the v3
  process-supervisor port lands.

## Shared seam introduced

`pvpgn::integration::legacy_d2dbs::bridge_logger()` mirrors
`legacy_bnetd::bridge_logger()`:

- Default sink: `core::default_logger()`.
- Test override: `BridgeLoggerOverride` RAII guard.
- Convenience inlines: `bridge_log` / `bridge_log_kv`.

The d2dbs and bnetd seams are intentionally separate namespaces so a
single binary that embeds both daemons (single-binary mode, see
docs/single-binary-mode.md) can override them independently.

## Files added (8 new files, 404 LOC)

- [src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/bridge_logger.hpp](src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/bridge_logger.hpp) -- NEW (76 LOC)
- [src/v3/integration/legacy_d2dbs/src/bridge_logger.cpp](src/v3/integration/legacy_d2dbs/src/bridge_logger.cpp) -- NEW (23 LOC)
- [src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/charlock_bridge.hpp](src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/charlock_bridge.hpp) -- NEW (18 LOC)
- [src/v3/integration/legacy_d2dbs/src/charlock_bridge.cpp](src/v3/integration/legacy_d2dbs/src/charlock_bridge.cpp) -- NEW (55 LOC)
- [src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/d2ladder_bridge.hpp](src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/d2ladder_bridge.hpp) -- NEW (17 LOC)
- [src/v3/integration/legacy_d2dbs/src/d2ladder_bridge.cpp](src/v3/integration/legacy_d2dbs/src/d2ladder_bridge.cpp) -- NEW (26 LOC)
- [tests/unit/integration/legacy_d2dbs/charlock_bridge_test.cpp](tests/unit/integration/legacy_d2dbs/charlock_bridge_test.cpp) -- NEW (97 LOC, 3 TEST_CASEs, 15 assertions)
- [tests/unit/integration/legacy_d2dbs/d2ladder_bridge_test.cpp](tests/unit/integration/legacy_d2dbs/d2ladder_bridge_test.cpp) -- NEW (92 LOC, 3 TEST_CASEs, 13 assertions)

## Files modified

- [src/v3/CMakeLists.txt](src/v3/CMakeLists.txt) -- 3 bridges added to `integration_legacy_d2dbs` SOURCES.
- [src/d2dbs/charlock.cpp](src/d2dbs/charlock.cpp) -- 2 guarded call sites + fwd decls.
- [src/d2dbs/d2ladder.cpp](src/d2dbs/d2ladder.cpp) -- 2 guarded call sites + fwd decls.
- [tests/unit/integration/legacy_d2dbs/CMakeLists.txt](tests/unit/integration/legacy_d2dbs/CMakeLists.txt) -- 2 `pvpgn_v3_add_test(...)` entries.
- [Dockerfile.v3](Dockerfile.v3) -- 2 targets added to v3-build `--target` list; 2 invocations added to v3-test stage.

## Verify (local)

```
cmake -S . -B build -D PVPGN_BUILD_V3=ON
cmake --build build --target \
    integration_legacy_d2dbs \
    test_integration_legacy_d2dbs_charlock_bridge \
    test_integration_legacy_d2dbs_d2ladder_bridge \
    d2dbs_legacy d2dbs -j$(nproc)

./build/tests/unit/integration/legacy_d2dbs/test_integration_legacy_d2dbs_charlock_bridge --reporter compact
./build/tests/unit/integration/legacy_d2dbs/test_integration_legacy_d2dbs_d2ladder_bridge --reporter compact
```

Result: 6 TEST_CASEs, 28 assertions, all GREEN.

The `d2dbs_legacy` static lib and the `d2dbs` executable both
link cleanly with `PVPGN_V3_D2DBS_INTEGRATION=1` -- proving the
forward declarations and guarded call sites are syntactically and
linker-correct end-to-end.

## Verify (CI)

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r231 .
```

(Not run locally for this round -- Docker step is the CI gate.)

## Lessons (memorialised)

- `core::ILogger::Field::value` is a `std::string_view`. Numeric
  arguments (e.g. `unsigned int tbllen`) must be rendered to a local
  buffer (`std::to_chars` into a `std::array<char, 20>`) BEFORE
  building the `Field`. Cf. `legacy_bnetd::userlog_bridge` which only
  needed string params.
- Forward declarations in legacy `src/d2dbs/*.cpp` files must be
  placed AFTER `#include "common/setup_after.h"`. The
  `setup_before.h` / `setup_after.h` brackets reshape several
  preprocessor symbols (notably `extern`-vs-static linkage on
  WIN32); declarations placed inside the brackets do not match the
  ABI of the corresponding definition compiled in the v3 lib (which
  is built without the brackets).
- d2dbs and bnetd MUST keep independent `bridge_logger` namespaces.
  Sharing the bnetd seam from a d2dbs bridge would entangle two
  daemons that single-binary mode runs in the same process.
