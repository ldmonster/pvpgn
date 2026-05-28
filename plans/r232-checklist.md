# R232 -- Observation bridges for d2dbs dupecheck + dbserver + signals

Status: **GREEN** (local Linux build: `cmake --build build --target d2dbs d2dbs_legacy test_integration_legacy_d2dbs_dbsdupecheck_bridge test_integration_legacy_d2dbs_dbserver_bridge test_integration_legacy_d2dbs_handle_signal_bridge`)

## Scope

Second strangler-fig round in the `src/d2dbs/` tree, continuing the
R230/R231 playbook. Adds three more observation-only bridge pairs.

| # | Module             | Op(s)                                                              |
|---|--------------------|--------------------------------------------------------------------|
| 1 | dbsdupecheck.cpp   | `dbsdupecheck(data, datalen)` (per-save call)                      |
| 2 | dbserver.cpp       | `dbs_server_main()`, `dbs_server_shutdown_connection(conn)`        |
| 3 | handle_signal.cpp  | `d2dbs_handle_signal_init()` (POSIX), `d2dbs_handle_signal()`      |

After R231 + R232 the `src/d2dbs/` instrumentation covers every
free-function entry point that the legacy build registers with the
d2dbs main loop, except `dbs_server_init` (covered indirectly via
`dbs_server_main`) and the helper schedulers inside `dbs_server_loop`
(which are not module-level entry points).

## Design notes

- The `dbsdupecheck` bridge MUST NOT inspect the `data` pointer
  contents -- the buffer is wire bytes lifted straight out of the
  legacy save packet, no sanitiser has run yet. We log only `datalen`
  and whether the pointer is null. Level is `Trace` (per-save calls
  are very chatty).
- The `dbs_server_shutdown_connection` bridge receives extracted
  scalars (`sd`, `serverid`, `type`, `verified`) instead of the
  `t_d2dbs_connection*` pointer. This keeps the bridge layer free of
  legacy types -- the bridge header lives in v3 with no `setup.h`
  include path.
- `d2dbs_handle_signal_init()` is POSIX-only; the WIN32 branch in
  `handle_signal.cpp` uses console-control wrappers. The R232 bridge
  is attached only to the POSIX branch. WIN32 instrumentation is
  deferred.
- All bridges return 0 (legacy fall-through). None mutates global
  state, none allocates, none throws.

## Files added (8 new files)

- [src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/dbsdupecheck_bridge.hpp](src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/dbsdupecheck_bridge.hpp)
- [src/v3/integration/legacy_d2dbs/src/dbsdupecheck_bridge.cpp](src/v3/integration/legacy_d2dbs/src/dbsdupecheck_bridge.cpp)
- [src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/dbserver_bridge.hpp](src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/dbserver_bridge.hpp)
- [src/v3/integration/legacy_d2dbs/src/dbserver_bridge.cpp](src/v3/integration/legacy_d2dbs/src/dbserver_bridge.cpp)
- [src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/handle_signal_bridge.hpp](src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/handle_signal_bridge.hpp)
- [src/v3/integration/legacy_d2dbs/src/handle_signal_bridge.cpp](src/v3/integration/legacy_d2dbs/src/handle_signal_bridge.cpp)
- [tests/unit/integration/legacy_d2dbs/dbsdupecheck_bridge_test.cpp](tests/unit/integration/legacy_d2dbs/dbsdupecheck_bridge_test.cpp) -- 3 TEST_CASEs, 12 assertions.
- [tests/unit/integration/legacy_d2dbs/dbserver_bridge_test.cpp](tests/unit/integration/legacy_d2dbs/dbserver_bridge_test.cpp) -- 3 TEST_CASEs, 17 assertions.
- [tests/unit/integration/legacy_d2dbs/handle_signal_bridge_test.cpp](tests/unit/integration/legacy_d2dbs/handle_signal_bridge_test.cpp) -- 3 TEST_CASEs, 14 assertions.

## Files modified

- [src/v3/CMakeLists.txt](src/v3/CMakeLists.txt) -- 3 bridge `.cpp` added to `integration_legacy_d2dbs`.
- [src/d2dbs/dbsdupecheck.cpp](src/d2dbs/dbsdupecheck.cpp) -- 1 guarded call site + fwd decl.
- [src/d2dbs/dbserver.cpp](src/d2dbs/dbserver.cpp) -- 2 guarded call sites + 2 fwd decls.
- [src/d2dbs/handle_signal.cpp](src/d2dbs/handle_signal.cpp) -- 2 guarded call sites + 2 fwd decls.
- [tests/unit/integration/legacy_d2dbs/CMakeLists.txt](tests/unit/integration/legacy_d2dbs/CMakeLists.txt) -- 3 `pvpgn_v3_add_test(...)` entries.
- [Dockerfile.v3](Dockerfile.v3) -- 3 targets added to v3-build `--target` list; 3 invocations added to v3-test stage.

## Verify (local)

```
cmake -S . -B build -D PVPGN_BUILD_V3=ON
cmake --build build --target \
    integration_legacy_d2dbs \
    test_integration_legacy_d2dbs_dbsdupecheck_bridge \
    test_integration_legacy_d2dbs_dbserver_bridge \
    test_integration_legacy_d2dbs_handle_signal_bridge \
    d2dbs_legacy d2dbs -j$(nproc)

./build/tests/unit/integration/legacy_d2dbs/test_integration_legacy_d2dbs_dbsdupecheck_bridge --reporter compact
./build/tests/unit/integration/legacy_d2dbs/test_integration_legacy_d2dbs_dbserver_bridge --reporter compact
./build/tests/unit/integration/legacy_d2dbs/test_integration_legacy_d2dbs_handle_signal_bridge --reporter compact
```

R232-only: 9 TEST_CASEs, 43 assertions, all GREEN.

Combined R231 + R232 d2dbs lifecycle suite: 15 TEST_CASEs, 71
assertions, all GREEN. `d2dbs_legacy` static lib and the `d2dbs`
executable both link cleanly with `PVPGN_V3_D2DBS_INTEGRATION=1`.

## Verify (CI)

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r232 .
```

(Not run locally for this round -- Docker step is the CI gate.)

## Lessons (memorialised)

- The d2dbs save path is hot. Per-call bridges on it MUST log at
  `Trace`, not `Debug`, to avoid flooding the default sink under load.
  Bridge call sites that fire once per connection (e.g.
  `dbs_server_shutdown_connection`) can stay at `Debug`.
- Bridge `.hpp` files MUST NOT include any legacy header (`dbserver.h`,
  `setup.h`, ...). The bridge signature accepts extracted POD scalars
  so the bridge translation unit compiles entirely in the v3 include
  graph and can be tested without `bnetd_legacy`/`d2dbs_legacy`
  pulled in.
- POSIX-only legacy entry points (e.g. `d2dbs_handle_signal_init`)
  can be instrumented with a single `#ifdef PVPGN_V3_D2DBS_INTEGRATION`
  guard inside the existing `#ifndef WIN32` block -- no extra
  conditional plumbing required.
