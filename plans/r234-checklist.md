# R234 -- More d2cs observation bridges (server_process + init + conn destroy)

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_server_bridge test_integration_legacy_d2cs_handle_init_bridge test_integration_legacy_d2cs_conn_bridge`)

## Scope

Second strangler-fig round in `src/d2cs/`, building on R233's
`legacy_d2cs::bridge_logger` seam. Instruments the d2cs main loop
entry, the wire-level connection classification dispatcher, and the
per-connection teardown path.

| # | Module             | Op(s)                                                       |
|---|--------------------|-------------------------------------------------------------|
| 1 | server.cpp         | `d2cs_server_process()` (event-loop entry, once per proc)   |
| 2 | handle_init.cpp    | `d2cs_handle_init_packet(c, packet)` (per init packet)      |
| 3 | connection.cpp     | `d2cs_conn_destroy(c, curr)` (per connection teardown)      |

After R233 + R234 the `src/d2cs/` instrumentation covers the full
d2cs lifecycle surface that the legacy build registers with the d2cs
main loop, plus the only two free-function entry points that fire
per-connection / per-init-packet without touching legacy types in the
v3 ABI.

## Design notes

- The `handle_init` bridge extracts the wire `cclass` byte BEFORE the
  bridge call so the bridge receives a settled scalar
  (`unsigned int`) -- the bridge does not touch `t_packet`.
- The `conn_destroy` bridge takes `(sd, sessionnum, cclass, state)`
  as scalars, mirroring the dbserver bridge from R232. The bridge
  never dereferences the `t_connection` pointer, so the header has no
  legacy dependencies.
- `d2cs_server_process()` is instrumented at the very top, before the
  POSIX `handle_signal_init()` call -- the v3 telemetry fires before
  any signal handlers are installed, so a future v3 event loop can
  observe the transition cleanly.
- Field-rendering for integers uses `std::to_chars` into local
  `std::array<char, 20>` buffers (same pattern as R232 dbserver
  bridge); the `Field::value` view stays valid until the
  `bridge_log_kv` call returns.

## Files added

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/server_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/server_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/handle_init_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/handle_init_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/conn_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/conn_bridge.cpp`
- `tests/unit/integration/legacy_d2cs/server_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/handle_init_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/conn_bridge_test.cpp`

## Files modified

- `src/v3/CMakeLists.txt` -- 3 new sources wired into
  `integration_legacy_d2cs` (parent lib, not `_linked` variant).
- `src/d2cs/server.cpp` -- forward decl after `setup_after.h`,
  guarded call at the very top of `d2cs_server_process()`.
- `src/d2cs/handle_init.cpp` -- forward decl after `setup_after.h`,
  guarded call in `d2cs_handle_init_packet()` immediately after the
  cclass byte is decoded (so both args are settled scalars).
- `src/d2cs/connection.cpp` -- forward decl after `setup_after.h`,
  guarded call near the top of `d2cs_conn_destroy()` (after the
  `conn_state_destroying` short-circuit; the connection is still
  intact at this point so its `sock`, `sessionnum`, `cclass`, and
  `state` fields are safe to read).
- `tests/unit/integration/legacy_d2cs/CMakeLists.txt` -- registers
  three new `pvpgn_v3_add_test(...)` entries.
- `Dockerfile.v3` -- three new targets added to both the explicit
  `cmake --build --target ...` list and the v3-test RUN chain.

## Verify

```sh
cmake -S . -B build
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_server_bridge \
    test_integration_legacy_d2cs_handle_init_bridge \
    test_integration_legacy_d2cs_conn_bridge \
    d2cs_legacy d2cs -j$(nproc)

./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_server_bridge      --reporter compact
./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_handle_init_bridge --reporter compact
./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_conn_bridge        --reporter compact
```

Result: 8 TEST_CASEs, 40 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean.

## Lessons (memorialised)

- For per-connection bridges, place the guarded call AFTER the
  `if (state == destroying) return 0;` short-circuit but BEFORE the
  hashtable removal -- the connection is still intact (all scalar
  fields are stable to read) and the bridge fires exactly once per
  real teardown, not once per re-entry attempt.
- For init-packet bridges, decode the wire byte FIRST and then pass
  it to the bridge. Don't let the bridge poke into `t_packet` -- that
  would force the bridge header to drag in legacy types and the v3
  integration lib would stop being legacy-header-free.
- For event-loop bridges, fire BEFORE the POSIX
  `handle_signal_init()` call so the order of operations in v3
  telemetry matches "loop entry, then signal install, then
  service-up".
